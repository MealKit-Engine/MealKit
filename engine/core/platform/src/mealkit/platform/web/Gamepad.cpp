// Browser gamepads: the Gamepad API (navigator.getGamepads) through emscripten/html5.h. Pads with the "standard"
// mapping report buttons in the Xbox layout (0 A, 1 B, 2 X, 3 Y ...); others are read as if they had it.
// Browsers list a pad only after one of its buttons was pressed while the page is open.
#include "mealkit/platform/Gamepad.h"

#include <emscripten/html5.h>

namespace mealkit {
namespace {

constexpr u16 kButtons[] = {pad::A,    pad::B,         pad::X,        pad::Y,           pad::LeftShoulder, pad::RightShoulder,
                            0,         0, // 6, 7: triggers (analog, below)
                            pad::Back, pad::Start,     pad::LeftThumb, pad::RightThumb, pad::DPadUp,      pad::DPadDown,
                            pad::DPadLeft, pad::DPadRight};

} // namespace

void Gamepads::update(f64) {
    for (PadState& p : m_pads) p = {};
    if (emscripten_sample_gamepad_data() != EMSCRIPTEN_RESULT_SUCCESS) return; // no Gamepad API
    const int count = emscripten_get_num_gamepads();
    u32 slot = 0;
    for (int i = 0; i < count && slot < kSlots; ++i) {
        EmscriptenGamepadEvent g{};
        if (emscripten_get_gamepad_status(i, &g) != EMSCRIPTEN_RESULT_SUCCESS || !g.connected) continue;
        PadState& p = m_pads[slot++];
        p.connected = true;
        for (int b = 0; b < g.numButtons && b < static_cast<int>(sizeof(kButtons) / sizeof(kButtons[0])); ++b)
            if (g.digitalButton[b] && kButtons[b]) p.buttons |= kButtons[b];
        if (g.numButtons > 7) {
            p.leftTrigger = static_cast<f32>(g.analogButton[6]);
            p.rightTrigger = static_cast<f32>(g.analogButton[7]);
        }
        // Axes: 0,1 left stick, 2,3 right stick, y is + downward
        if (g.numAxes >= 2) {
            p.leftX = static_cast<f32>(g.axis[0]);
            p.leftY = -static_cast<f32>(g.axis[1]);
        }
        if (g.numAxes >= 4) {
            p.rightX = static_cast<f32>(g.axis[2]);
            p.rightY = -static_cast<f32>(g.axis[3]);
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
