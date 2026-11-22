// Linux gamepads: evdev (/dev/input/event*). Desktop sessions give the seat user access to joysticks (udev uaccess).
// Buttons follow the Linux gamepad layout (BTN_SOUTH = A ...), which xpad, hid-playstation and SDL-style pads all report.
#include <cerrno>
#include <cstring>
#include <string>

#include "mealkit/platform/Gamepad.h"

#include "mealkit/core/Log.h"

#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <unistd.h>

namespace mealkit {
namespace {

constexpr f64 kDisconnectedCheckSeconds = 1.0;

struct Axis {
    i32 min = 0, max = 0;
    bool present = false;
};

struct Device {
    int fd = -1;
    dev_t id = 0;
    Axis axes[ABS_CNT];
};

struct Devices {
    Device slots[Gamepads::kSlots];
};

bool testBit(const unsigned long* bits, unsigned bit) {
    constexpr unsigned kBits = sizeof(unsigned long) * 8;
    return (bits[bit / kBits] >> (bit % kBits)) & 1ul;
}

bool isGamepad(int fd) {
    unsigned long keys[(KEY_CNT + 63) / 64] = {};
    unsigned long abs[(ABS_CNT + 63) / 64] = {};
    if (ioctl(fd, EVIOCGBIT(EV_KEY, sizeof(keys)), keys) < 0) return false;
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs)), abs) < 0) return false;
    return testBit(keys, BTN_GAMEPAD) && testBit(abs, ABS_X) && testBit(abs, ABS_Y);
}

// -1..1 from the device range (centered); triggers use 0..1 (trigger = true)
f32 normalize(const Axis& a, i32 value, bool trigger) {
    if (!a.present || a.max <= a.min) return 0.0f;
    const f32 t = static_cast<f32>(value - a.min) / static_cast<f32>(a.max - a.min);
    return trigger ? t : t * 2.0f - 1.0f;
}

u16 buttonBit(u16 code) {
    switch (code) {
    case BTN_SOUTH: return pad::A;
    case BTN_EAST: return pad::B;
    case BTN_NORTH: return pad::X; // BTN_X
    case BTN_WEST: return pad::Y;  // BTN_Y
    case BTN_TL: return pad::LeftShoulder;
    case BTN_TR: return pad::RightShoulder;
    case BTN_SELECT: return pad::Back;
    case BTN_START: return pad::Start;
    case BTN_THUMBL: return pad::LeftThumb;
    case BTN_THUMBR: return pad::RightThumb;
    case BTN_DPAD_UP: return pad::DPadUp;
    case BTN_DPAD_DOWN: return pad::DPadDown;
    case BTN_DPAD_LEFT: return pad::DPadLeft;
    case BTN_DPAD_RIGHT: return pad::DPadRight;
    default: return 0;
    }
}

bool alreadyOpen(const Devices& d, dev_t id) {
    for (const Device& s : d.slots)
        if (s.fd >= 0 && s.id == id) return true;
    return false;
}

// Opens one not yet used gamepad into slot (false if none)
bool openNext(Devices& d, Device& slot) {
    DIR* dir = opendir("/dev/input");
    if (!dir) return false;
    bool found = false;
    while (dirent* entry = readdir(dir)) {
        if (std::strncmp(entry->d_name, "event", 5) != 0) continue;
        const std::string path = std::string("/dev/input/") + entry->d_name;
        struct stat st {};
        if (stat(path.c_str(), &st) != 0 || alreadyOpen(d, st.st_rdev)) continue;
        const int fd = open(path.c_str(), O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) continue;
        if (!isGamepad(fd)) {
            ::close(fd);
            continue;
        }
        slot = {};
        slot.fd = fd;
        slot.id = st.st_rdev;
        for (unsigned a = 0; a < ABS_CNT; ++a) {
            input_absinfo info{};
            if (ioctl(fd, EVIOCGABS(a), &info) == 0 && info.maximum > info.minimum)
                slot.axes[a] = {info.minimum, info.maximum, true};
        }
        found = true;
        break;
    }
    closedir(dir);
    return found;
}

} // namespace

void Gamepads::update(f64 dt) {
    if (!m_platform) m_platform = new Devices;
    Devices& devices = *static_cast<Devices*>(m_platform);
    for (u32 i = 0; i < kSlots; ++i) {
        PadState& p = m_pads[i];
        Device& d = devices.slots[i];
        if (d.fd < 0) {
            m_untilCheck[i] -= dt;
            if (m_untilCheck[i] > 0.0) continue;
            m_untilCheck[i] = kDisconnectedCheckSeconds;
            if (!openNext(devices, d)) continue;
            p = {};
            p.connected = true;
            MEALKIT_INFO("Gamepad %u connected", i);
        }
        input_event events[64];
        for (;;) {
            const ssize_t n = read(d.fd, events, sizeof(events));
            if (n < 0 && errno == EAGAIN) break;
            if (n <= 0) { // unplugged (ENODEV)
                ::close(d.fd);
                d = {};
                p = {};
                MEALKIT_INFO("Gamepad %u disconnected", i);
                break;
            }
            for (size_t e = 0; e < static_cast<size_t>(n) / sizeof(input_event); ++e) {
                const input_event& ev = events[e];
                if (ev.type == EV_KEY) {
                    const u16 bit = buttonBit(ev.code);
                    p.buttons = ev.value ? static_cast<u16>(p.buttons | bit) : static_cast<u16>(p.buttons & ~bit);
                } else if (ev.type == EV_ABS && ev.code < ABS_CNT) {
                    const Axis& a = d.axes[ev.code];
                    switch (ev.code) {
                    case ABS_X: p.leftX = normalize(a, ev.value, false); break;
                    case ABS_Y: p.leftY = -normalize(a, ev.value, false); break; // evdev y is + downward
                    case ABS_RX: p.rightX = normalize(a, ev.value, false); break;
                    case ABS_RY: p.rightY = -normalize(a, ev.value, false); break;
                    case ABS_Z: p.leftTrigger = normalize(a, ev.value, true); break;
                    case ABS_RZ: p.rightTrigger = normalize(a, ev.value, true); break;
                    case ABS_HAT0X:
                        p.buttons &= static_cast<u16>(~(pad::DPadLeft | pad::DPadRight));
                        if (ev.value < 0) p.buttons |= pad::DPadLeft;
                        if (ev.value > 0) p.buttons |= pad::DPadRight;
                        break;
                    case ABS_HAT0Y:
                        p.buttons &= static_cast<u16>(~(pad::DPadUp | pad::DPadDown));
                        if (ev.value < 0) p.buttons |= pad::DPadUp;
                        if (ev.value > 0) p.buttons |= pad::DPadDown;
                        break;
                    default: break;
                    }
                }
            }
        }
    }
}

const PadState& Gamepads::first() const {
    static const PadState none{};
    for (const PadState& p : m_pads)
        if (p.connected) return p;
    return none;
}

u32 Gamepads::connectedCount() const {
    u32 n = 0;
    for (const PadState& p : m_pads) n += p.connected ? 1u : 0u;
    return n;
}

} // namespace mealkit
