#pragma once

#include "mealkit/core/Types.h"
#include "mealkit/platform/Input.h"

#include <cstdint>
#include <vector>

namespace mealkit {

constexpr u32 kDefaultCaptionColor = 0xFFFFFFFFu;

struct WindowDesc {
    const char* title = "MealKit";
    u32 width = 1280; // client area size (physical pixels)
    u32 height = 720;
    // Title bar color (0xRRGGBB). If set, turns on the dark title bar (white text/buttons), and on Windows 11 paints it in this color
    u32 captionColor = kDefaultCaptionColor;
};

// Window mode (settings screen). Fullscreen is a borderless window the size of the monitor (flip model swap chains have the same latency without exclusive fullscreen)
enum class WindowMode : u8 { Windowed, Borderless, Fullscreen };

struct DisplaySize {
    u32 width = 0, height = 0;
};

// Position and size of the restored window plus the maximized flag (width 0 = none saved)
struct WindowPlacement {
    i32 x = 0, y = 0, width = 0, height = 0;
    bool maximized = false;
};

// Hook letting other modules (editor UI etc.) see window messages first. Returning nonzero treats the message as handled.
using MessageHook = std::intptr_t (*)(void* user, void* hwnd, u32 msg, std::uintptr_t wparam, std::intptr_t lparam);

class Window {
public:
    Window() = default;
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool create(const WindowDesc& desc);

    // Processes messages and updates input state. false when the window is closed.
    bool pumpMessages();

    void* nativeHandle() const { return m_hwnd; } // HWND
    u32 width() const { return m_width; }
    u32 height() const { return m_height; }
    bool minimized() const { return m_minimized; }
    bool focused() const { return m_focused; }
    bool consumeResize(); // true only once after the size changes
    void close();         // the next pumpMessages returns false (the game menu's [Quit])

    void setTitle(const char* utf8);
    void setCursorCaptured(bool captured); // hide cursor + confine to the window (for camera rotation)
    // Change window mode/size. Only windowed mode uses width×height (client area); others use the whole monitor the window is on
    void setMode(WindowMode mode, u32 width, u32 height);
    WindowMode mode() const { return m_mode; }
    // Resolutions supported by the monitor the window is on (largest first, no duplicates)
    std::vector<DisplaySize> displaySizes() const;
    // Saved and restored between runs. The OS moves a placement from a missing monitor back on screen
    WindowPlacement placement() const;
    void setPlacement(const WindowPlacement& placement);

    Input& input() { return m_input; }
    void setMessageHook(MessageHook hook, void* user);

    // Called from WndProc.
    std::intptr_t handleMessage(void* hwnd, u32 msg, std::uintptr_t wparam, std::intptr_t lparam);

private:
    void* m_hwnd = nullptr;
    u32 m_width = 0;
    u32 m_height = 0;
    bool m_minimized = false;
    bool m_focused = true;
    bool m_resized = false;
    bool m_quit = false;
    bool m_captured = false;
    u32 m_highSurrogate = 0; // WM_CHAR: the first half of a UTF-16 character arriving in two parts
    WindowMode m_mode = WindowMode::Windowed;
    Input m_input;
    MessageHook m_hook = nullptr;
    void* m_hookUser = nullptr;
};

} // namespace mealkit
