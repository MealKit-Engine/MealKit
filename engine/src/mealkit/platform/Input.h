#pragma once

#include "mealkit/core/Types.h"

namespace mealkit {

// Win32 가상 키 코드. 글자 키는 'W' 처럼 대문자 문자를 그대로 쓴다.
namespace key {
inline constexpr u8 Backspace = 0x08;
inline constexpr u8 Tab = 0x09;
inline constexpr u8 Enter = 0x0D;
inline constexpr u8 Shift = 0x10;
inline constexpr u8 Control = 0x11;
inline constexpr u8 Alt = 0x12;
inline constexpr u8 Escape = 0x1B;
inline constexpr u8 Space = 0x20;
inline constexpr u8 Left = 0x25;
inline constexpr u8 Up = 0x26;
inline constexpr u8 Right = 0x27;
inline constexpr u8 Down = 0x28;
inline constexpr u8 F1 = 0x70;
inline constexpr u8 F2 = 0x71;
inline constexpr u8 F3 = 0x72;
inline constexpr u8 F4 = 0x73;
inline constexpr u8 F11 = 0x7A;
inline constexpr u8 F12 = 0x7B;
} // namespace key

enum class MouseButton : u8 { Left = 0, Right = 1, Middle = 2 };

class Input {
public:
    bool keyDown(u8 vk) const { return m_keys[vk]; }
    bool keyPressed(u8 vk) const { return m_keys[vk] && !m_prevKeys[vk]; }
    bool mouseDown(MouseButton b) const { return m_mouse[static_cast<u8>(b)]; }
    bool mousePressed(MouseButton b) const {
        const u8 i = static_cast<u8>(b);
        return m_mouse[i] && !m_prevMouse[i];
    }
    f32 mouseDeltaX() const { return m_dx; } // Raw Input: 가속 없는 픽셀 단위 이동량
    f32 mouseDeltaY() const { return m_dy; }
    f32 wheelDelta() const { return m_wheel; } // 노치 단위 (+ = 위로)

    // --- Window 내부 사용 ---
    void beginFrame() {
        for (int i = 0; i < 256; ++i) m_prevKeys[i] = m_keys[i];
        for (int i = 0; i < 3; ++i) m_prevMouse[i] = m_mouse[i];
        m_dx = m_dy = m_wheel = 0.0f;
    }
    void setKey(u8 vk, bool down) { m_keys[vk] = down; }
    void setMouse(MouseButton b, bool down) { m_mouse[static_cast<u8>(b)] = down; }
    void addMouseDelta(f32 dx, f32 dy) {
        m_dx += dx;
        m_dy += dy;
    }
    void addWheel(f32 notches) { m_wheel += notches; }
    void clear() {
        for (bool& k : m_keys) k = false;
        for (bool& b : m_mouse) b = false;
    }

private:
    bool m_keys[256] = {};
    bool m_prevKeys[256] = {};
    bool m_mouse[3] = {};
    bool m_prevMouse[3] = {};
    f32 m_dx = 0.0f, m_dy = 0.0f, m_wheel = 0.0f;
};

} // namespace mealkit
