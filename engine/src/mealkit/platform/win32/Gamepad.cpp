#include "mealkit/platform/Gamepad.h"

#include "mealkit/core/Log.h"

#include <windows.h>
#include <xinput.h>

namespace mealkit {
namespace {

constexpr f64 kDisconnectedCheckSeconds = 1.0;

f32 stick(SHORT v) { return v < 0 ? static_cast<f32>(v) / 32768.0f : static_cast<f32>(v) / 32767.0f; }

u16 buttons(WORD w) {
    u16 b = 0;
    if (w & XINPUT_GAMEPAD_A) b |= input::pad::A;
    if (w & XINPUT_GAMEPAD_B) b |= input::pad::B;
    if (w & XINPUT_GAMEPAD_X) b |= input::pad::X;
    if (w & XINPUT_GAMEPAD_Y) b |= input::pad::Y;
    if (w & XINPUT_GAMEPAD_LEFT_SHOULDER) b |= input::pad::LeftShoulder;
    if (w & XINPUT_GAMEPAD_RIGHT_SHOULDER) b |= input::pad::RightShoulder;
    if (w & XINPUT_GAMEPAD_BACK) b |= input::pad::Back;
    if (w & XINPUT_GAMEPAD_START) b |= input::pad::Start;
    if (w & XINPUT_GAMEPAD_LEFT_THUMB) b |= input::pad::LeftThumb;
    if (w & XINPUT_GAMEPAD_RIGHT_THUMB) b |= input::pad::RightThumb;
    if (w & XINPUT_GAMEPAD_DPAD_UP) b |= input::pad::DPadUp;
    if (w & XINPUT_GAMEPAD_DPAD_DOWN) b |= input::pad::DPadDown;
    if (w & XINPUT_GAMEPAD_DPAD_LEFT) b |= input::pad::DPadLeft;
    if (w & XINPUT_GAMEPAD_DPAD_RIGHT) b |= input::pad::DPadRight;
    return b;
}

} // namespace

void Gamepads::update(f64 dt) {
    for (u32 i = 0; i < kSlots; ++i) {
        input::PadState& p = m_pads[i];
        if (!p.connected) {
            m_untilCheck[i] -= dt;
            if (m_untilCheck[i] > 0.0) continue;
            m_untilCheck[i] = kDisconnectedCheckSeconds;
        }
        XINPUT_STATE s{};
        if (XInputGetState(i, &s) != ERROR_SUCCESS) {
            if (p.connected) MEALKIT_INFO("Gamepad %u disconnected", i);
            p = {};
            continue;
        }
        if (!p.connected) MEALKIT_INFO("Gamepad %u connected", i);
        const XINPUT_GAMEPAD& g = s.Gamepad;
        p.connected = true;
        p.buttons = buttons(g.wButtons);
        p.leftX = stick(g.sThumbLX);
        p.leftY = stick(g.sThumbLY);
        p.rightX = stick(g.sThumbRX);
        p.rightY = stick(g.sThumbRY);
        p.leftTrigger = static_cast<f32>(g.bLeftTrigger) / 255.0f;
        p.rightTrigger = static_cast<f32>(g.bRightTrigger) / 255.0f;
    }
}

const input::PadState& Gamepads::first() const {
    static const input::PadState none{};
    for (const input::PadState& p : m_pads)
        if (p.connected) return p;
    return none;
}

u32 Gamepads::connectedCount() const {
    u32 n = 0;
    for (const input::PadState& p : m_pads) n += p.connected ? 1u : 0u;
    return n;
}

} // namespace mealkit
