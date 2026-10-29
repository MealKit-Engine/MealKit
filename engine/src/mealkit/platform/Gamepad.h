#pragma once

#include "mealkit/core/Types.h"

// Gamepads (platform layer). Windows uses XInput (Xbox/XInput-compatible pads, 4 slots).
// Reading unconnected slots is slow (about 14 µs per slot, docs/DECISIONS.md 2026-10-18), so they're checked only once per second.
// The engine (shipping exe, editor play, join windows) makes one, updates it every frame, and passes the first connected pad to input actions.
namespace mealkit {

// State of one gamepad (filled by Gamepads and read by input actions input::Actions). Stick y is + upward.
namespace pad {
inline constexpr u16 A = 1u << 0, B = 1u << 1, X = 1u << 2, Y = 1u << 3;
inline constexpr u16 LeftShoulder = 1u << 4, RightShoulder = 1u << 5, Back = 1u << 6, Start = 1u << 7;
inline constexpr u16 LeftThumb = 1u << 8, RightThumb = 1u << 9;
inline constexpr u16 DPadUp = 1u << 10, DPadDown = 1u << 11, DPadLeft = 1u << 12, DPadRight = 1u << 13;
} // namespace pad

struct PadState {
    bool connected = false;
    u16 buttons = 0; // pad::A ...
    f32 leftX = 0.0f, leftY = 0.0f, rightX = 0.0f, rightY = 0.0f; // -1..1 (before deadzone)
    f32 leftTrigger = 0.0f, rightTrigger = 0.0f;                 // 0..1
};

class Gamepads {
public:
    static constexpr u32 kSlots = 4;

    // Once per frame. dt in seconds (counts the connection check interval)
    void update(f64 dt);
    const PadState& state(u32 slot) const { return m_pads[slot]; }
    // First connected pad (disconnected state if none). Used by single-player-per-PC games
    const PadState& first() const;
    u32 connectedCount() const;

private:
    PadState m_pads[kSlots];
    f64 m_untilCheck[kSlots] = {}; // unconnected slots: seconds left until the next check
};

} // namespace mealkit
