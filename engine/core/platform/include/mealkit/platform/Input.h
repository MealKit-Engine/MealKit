#pragma once

#include "mealkit/core/Types.h"

#include <string>

namespace mealkit {

// Win32 virtual key codes. Letter keys use the uppercase character as is, like 'W'.
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
inline constexpr u8 F9 = 0x78;
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
    f32 mouseDeltaX() const { return m_dx; } // Raw Input: pixel movement without acceleration
    f32 mouseDeltaY() const { return m_dy; }
    f32 wheelDelta() const { return m_wheel; } // in notches (+ = up)
    // Cursor position (pixels inside the window, top left is 0). Whether it moved this frame
    f32 mouseX() const { return m_x; }
    f32 mouseY() const { return m_y; }
    bool mouseMoved() const { return m_moved; }
    // Text typed this frame (UTF-8, including characters finished composing in the Korean IME, no control characters) and backspace count (including key repeat). For text input widgets
    const std::string& text() const { return m_text; }
    u32 backspaces() const { return m_backspaces; }

    // --- Used internally by Window ---
    void beginFrame() {
        for (int i = 0; i < 256; ++i) m_prevKeys[i] = m_keys[i];
        for (int i = 0; i < 3; ++i) m_prevMouse[i] = m_mouse[i];
        m_dx = m_dy = m_wheel = 0.0f;
        m_moved = false;
        m_text.clear();
        m_backspaces = 0;
    }
    void addText(u32 codepoint) { // one character (appended as UTF-8)
        if (codepoint == 8) {
            ++m_backspaces;
            return;
        }
        if (codepoint < 0x20 || codepoint == 0x7F || codepoint > 0x10FFFF) return;
        if (codepoint < 0x80) {
            m_text += static_cast<char>(codepoint);
        } else if (codepoint < 0x800) {
            m_text += static_cast<char>(0xC0 | (codepoint >> 6));
            m_text += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else if (codepoint < 0x10000) {
            m_text += static_cast<char>(0xE0 | (codepoint >> 12));
            m_text += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            m_text += static_cast<char>(0x80 | (codepoint & 0x3F));
        } else {
            m_text += static_cast<char>(0xF0 | (codepoint >> 18));
            m_text += static_cast<char>(0x80 | ((codepoint >> 12) & 0x3F));
            m_text += static_cast<char>(0x80 | ((codepoint >> 6) & 0x3F));
            m_text += static_cast<char>(0x80 | (codepoint & 0x3F));
        }
    }
    void setKey(u8 vk, bool down) { m_keys[vk] = down; }
    void setMouse(MouseButton b, bool down) { m_mouse[static_cast<u8>(b)] = down; }
    void addMouseDelta(f32 dx, f32 dy) {
        m_dx += dx;
        m_dy += dy;
    }
    void addWheel(f32 notches) { m_wheel += notches; }
    void setMousePosition(f32 x, f32 y) {
        m_moved = m_moved || x != m_x || y != m_y;
        m_x = x;
        m_y = y;
    }
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
    f32 m_x = -1.0f, m_y = -1.0f;
    bool m_moved = false;
    std::string m_text;
    u32 m_backspaces = 0;
};

} // namespace mealkit
