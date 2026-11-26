// Browser window: the page's <canvas id="canvas"> (cmake/web/shell.html), sized to its CSS box times devicePixelRatio.
// Input comes from DOM events (emscripten/html5.h). They arrive while the game waits for the next animation frame
// (JSPI), so they are queued and applied in pumpMessages after Input::beginFrame, like X11/Win32 messages.
#include "mealkit/platform/Window.h"

#include "mealkit/core/Log.h"

#include <cmath>
#include <cstring>
#include <vector>

#include <emscripten/emscripten.h>
#include <emscripten/html5.h>

namespace mealkit {
namespace {

constexpr const char* kCanvas = "#canvas";

enum class EventKind : u8 { Key, Text, Button, Move, Delta, Wheel, Blur, Focus, Resize, Unlock };

struct Event {
    EventKind kind;
    bool down = false;
    u32 value = 0; // key code, code point, mouse button
    f32 x = 0.0f, y = 0.0f;
};

struct WebState {
    WebCanvasHandle canvas; // what nativeHandle() points at
    std::vector<Event> events;
    bool pointerLocked = false;
};

WebState* state(void* p) { return static_cast<WebState*>(p); }

// KeyboardEvent.keyCode is the Win32 virtual key code for letters, digits, F keys, arrows and the keys in key::
u8 vkFromKeyCode(unsigned long code) { return code < 256 ? static_cast<u8>(code) : 0; }

// One code point from a UTF-8 string, 0 if it is more than one character ("Shift", "ArrowLeft" ...)
u32 singleCodepoint(const char* s) {
    const auto* p = reinterpret_cast<const unsigned char*>(s);
    if (!p[0]) return 0;
    u32 c = p[0];
    int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
    if (extra) c &= 0x3F >> extra;
    int i = 1;
    for (; extra > 0; --extra, ++i) {
        if ((p[i] & 0xC0) != 0x80) return 0;
        c = (c << 6) | (p[i] & 0x3F);
    }
    return p[i] ? 0 : c;
}

f32 pixelRatio() { return static_cast<f32>(emscripten_get_device_pixel_ratio()); }

EM_BOOL onKey(int type, const EmscriptenKeyboardEvent* e, void* user) {
    WebState& w = *state(user);
    const bool down = type == EMSCRIPTEN_EVENT_KEYDOWN;
    if (const u8 vk = vkFromKeyCode(e->keyCode)) w.events.push_back({EventKind::Key, down, vk});
    if (down && !e->ctrlKey && !e->altKey && !e->metaKey) {
        if (e->keyCode == 8) w.events.push_back({EventKind::Text, true, 8});
        else if (const u32 c = singleCodepoint(e->key)) w.events.push_back({EventKind::Text, true, c});
    }
    // The page doesn't scroll or go back on game keys; browser reload (F5) and developer tools (F12) stay
    return e->keyCode != 116 && e->keyCode != 123 && !(e->ctrlKey && (e->keyCode == 'R' || e->keyCode == 'C' || e->keyCode == 'V'));
}

EM_BOOL onMouse(int type, const EmscriptenMouseEvent* e, void* user) {
    WebState& w = *state(user);
    const f32 ratio = pixelRatio();
    const f32 x = static_cast<f32>(e->targetX) * ratio, y = static_cast<f32>(e->targetY) * ratio;
    if (type == EMSCRIPTEN_EVENT_MOUSEMOVE) {
        w.events.push_back({EventKind::Move, false, 0, x, y});
        w.events.push_back({EventKind::Delta, false, 0, static_cast<f32>(e->movementX) * ratio, static_cast<f32>(e->movementY) * ratio});
        return EM_TRUE;
    }
    // DOM buttons: 0 left, 1 middle, 2 right
    const u32 button = e->button == 0 ? 0u : e->button == 2 ? 1u : e->button == 1 ? 2u : 99u;
    if (button < 3) w.events.push_back({EventKind::Button, type == EMSCRIPTEN_EVENT_MOUSEDOWN, button, x, y});
    return EM_TRUE;
}

EM_BOOL onWheel(int, const EmscriptenWheelEvent* e, void* user) {
    // Notches (+ = up): pixel deltas count 100 per notch, line deltas 3
    const f64 per = e->deltaMode == DOM_DELTA_PIXEL ? 100.0 : e->deltaMode == DOM_DELTA_LINE ? 3.0 : 1.0;
    state(user)->events.push_back({EventKind::Wheel, false, 0, static_cast<f32>(-e->deltaY / per)});
    return EM_TRUE;
}

EM_BOOL onFocus(int type, const EmscriptenFocusEvent*, void* user) {
    state(user)->events.push_back({type == EMSCRIPTEN_EVENT_BLUR ? EventKind::Blur : EventKind::Focus});
    return EM_FALSE;
}

EM_BOOL onResize(int, const EmscriptenUiEvent*, void* user) {
    state(user)->events.push_back({EventKind::Resize});
    return EM_FALSE;
}

EM_BOOL onPointerLock(int, const EmscriptenPointerlockChangeEvent* e, void* user) {
    WebState& w = *state(user);
    if (w.pointerLocked && !e->isActive) w.events.push_back({EventKind::Unlock}); // the user ended it (Esc)
    w.pointerLocked = e->isActive;
    return EM_FALSE;
}

// Drawing buffer size = the canvas's CSS size in physical pixels
void canvasSize(u32& width, u32& height) {
    double cssW = 0.0, cssH = 0.0;
    emscripten_get_element_css_size(kCanvas, &cssW, &cssH);
    const f64 ratio = emscripten_get_device_pixel_ratio();
    width = static_cast<u32>(std::max(1.0, std::round(cssW * ratio)));
    height = static_cast<u32>(std::max(1.0, std::round(cssH * ratio)));
}

} // namespace

Window::~Window() {
    WebState* w = state(m_platform);
    if (!w) return;
    emscripten_html5_remove_all_event_listeners();
    delete w;
}

bool Window::create(const WindowDesc& desc) {
    auto* w = new WebState;
    w->canvas.selector = kCanvas;
    m_platform = w;
    m_hwnd = &w->canvas;
    canvasSize(m_width, m_height);
    if (emscripten_set_canvas_element_size(kCanvas, static_cast<int>(m_width), static_cast<int>(m_height)) != EMSCRIPTEN_RESULT_SUCCESS) {
        MEALKIT_ERROR("No <canvas id=\"canvas\"> on the page");
        return false;
    }
    const char* target = EMSCRIPTEN_EVENT_TARGET_WINDOW;
    emscripten_set_keydown_callback(target, w, true, onKey);
    emscripten_set_keyup_callback(target, w, true, onKey);
    emscripten_set_mousedown_callback(kCanvas, w, true, onMouse);
    emscripten_set_mouseup_callback(target, w, true, onMouse); // a release outside the canvas still ends the drag
    emscripten_set_mousemove_callback(target, w, true, onMouse);
    emscripten_set_wheel_callback(kCanvas, w, true, onWheel);
    emscripten_set_blur_callback(target, w, true, onFocus);
    emscripten_set_focus_callback(target, w, true, onFocus);
    emscripten_set_resize_callback(target, w, true, onResize);
    emscripten_set_pointerlockchange_callback(EMSCRIPTEN_EVENT_TARGET_DOCUMENT, w, true, onPointerLock);
    setTitle(desc.title);
    return true;
}

void Window::setMode(WindowMode mode, u32, u32) {
    m_mode = mode;
    // Borderless and Fullscreen are the browser's fullscreen (allowed after a click or key press; deferred until then)
    if (mode == WindowMode::Windowed) {
        emscripten_exit_fullscreen();
        return;
    }
    EmscriptenFullscreenStrategy s{};
    s.scaleMode = EMSCRIPTEN_FULLSCREEN_SCALE_STRETCH;
    s.canvasResolutionScaleMode = EMSCRIPTEN_FULLSCREEN_CANVAS_SCALE_HIDEF;
    s.filteringMode = EMSCRIPTEN_FULLSCREEN_FILTERING_DEFAULT;
    emscripten_request_fullscreen_strategy(kCanvas, true, &s);
}

std::vector<DisplaySize> Window::displaySizes() const {
    // The screen the page is on (the browser picks the mode; fullscreen always fills it)
    const f64 ratio = emscripten_get_device_pixel_ratio();
    const int w = EM_ASM_INT({ return screen.width; }), h = EM_ASM_INT({ return screen.height; });
    return {{static_cast<u32>(w * ratio), static_cast<u32>(h * ratio)}};
}

WindowPlacement Window::placement() const { return {}; } // the page decides where the canvas is

void Window::setPlacement(const WindowPlacement&) {}

bool Window::pumpMessages() {
    m_input.beginFrame();
    WebState* w = state(m_platform);
    if (!w) return !m_quit;
    std::vector<Event> events;
    events.swap(w->events);
    // A key or button released in the same frame it went down (a tap between two frames) is released next frame,
    // so keyPressed/mousePressed still see it
    bool downNow[256] = {}, buttonNow[3] = {};
    std::vector<Event> later;
    for (const Event& e : events) {
        if (e.kind == EventKind::Key) {
            if (!e.down && downNow[e.value]) {
                later.push_back(e);
                continue;
            }
            if (e.down) downNow[e.value] = true;
        } else if (e.kind == EventKind::Button) {
            if (!e.down && buttonNow[e.value]) {
                later.push_back(e);
                continue;
            }
            if (e.down) buttonNow[e.value] = true;
        }
        handleMessage(nullptr, static_cast<u32>(e.kind), 0, reinterpret_cast<std::intptr_t>(&e));
    }
    w->events.insert(w->events.begin(), later.begin(), later.end());
    return !m_quit;
}

void Window::close() { m_quit = true; } // the page stays; the game loop ends

bool Window::consumeResize() {
    const bool r = m_resized;
    m_resized = false;
    return r;
}

void Window::setTitle(const char* utf8) { emscripten_set_window_title(utf8); }

void Window::setCursorCaptured(bool captured) {
    if (captured == m_captured || !m_platform) return;
    m_captured = captured;
    // Pointer lock: hidden cursor and movement without limits. The browser allows it only after a user gesture (deferred)
    if (captured) emscripten_request_pointerlock(kCanvas, true);
    else emscripten_exit_pointerlock();
}

void Window::setMessageHook(MessageHook hook, void* user) {
    m_hook = hook;
    m_hookUser = user;
}

std::intptr_t Window::handleMessage(void* hwnd, u32 msg, std::uintptr_t wparam, std::intptr_t lparam) {
    if (m_hook) {
        const std::intptr_t handled = m_hook(m_hookUser, hwnd, msg, wparam, lparam);
        if (handled) return handled;
    }
    const Event& e = *reinterpret_cast<const Event*>(lparam);
    switch (e.kind) {
    case EventKind::Key: m_input.setKey(static_cast<u8>(e.value), e.down); break;
    case EventKind::Text: m_input.addText(e.value); break;
    case EventKind::Button:
        m_input.setMouse(static_cast<MouseButton>(e.value), e.down);
        m_input.setMousePosition(e.x, e.y);
        break;
    case EventKind::Move: m_input.setMousePosition(e.x, e.y); break;
    case EventKind::Delta:
        if (m_focused) m_input.addMouseDelta(e.x, e.y);
        break;
    case EventKind::Wheel: m_input.addWheel(e.x); break;
    case EventKind::Blur:
        m_focused = false;
        m_input.clear();
        setCursorCaptured(false);
        break;
    case EventKind::Focus: m_focused = true; break;
    case EventKind::Unlock: m_captured = false; break; // released: the game's next capture asks again
    case EventKind::Resize: {
        u32 width = 0, height = 0;
        canvasSize(width, height);
        if (width != m_width || height != m_height) {
            emscripten_set_canvas_element_size(kCanvas, static_cast<int>(width), static_cast<int>(height));
            m_width = width;
            m_height = height;
            m_resized = true;
        }
        break;
    }
    }
    return 0;
}

} // namespace mealkit
