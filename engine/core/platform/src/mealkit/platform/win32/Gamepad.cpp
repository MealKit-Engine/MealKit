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
    if (w & XINPUT_GAMEPAD_A) b |= pad::A;
    if (w & XINPUT_GAMEPAD_B) b |= pad::B;
    if (w & XINPUT_GAMEPAD_X) b |= pad::X;
    if (w & XINPUT_GAMEPAD_Y) b |= pad::Y;
    if (w & XINPUT_GAMEPAD_LEFT_SHOULDER) b |= pad::LeftShoulder;
    if (w & XINPUT_GAMEPAD_RIGHT_SHOULDER) b |= pad::RightShoulder;
    if (w & XINPUT_GAMEPAD_BACK) b |= pad::Back;
    if (w & XINPUT_GAMEPAD_START) b |= pad::Start;
    if (w & XINPUT_GAMEPAD_LEFT_THUMB) b |= pad::LeftThumb;
    if (w & XINPUT_GAMEPAD_RIGHT_THUMB) b |= pad::RightThumb;
    if (w & XINPUT_GAMEPAD_DPAD_UP) b |= pad::DPadUp;
    if (w & XINPUT_GAMEPAD_DPAD_DOWN) b |= pad::DPadDown;
    if (w & XINPUT_GAMEPAD_DPAD_LEFT) b |= pad::DPadLeft;
    if (w & XINPUT_GAMEPAD_DPAD_RIGHT) b |= pad::DPadRight;
    return b;
}

} // namespace

void Gamepads::update(f64 dt) {
    for (u32 i = 0; i < kSlots; ++i) {
        PadState& p = m_pads[i];
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
