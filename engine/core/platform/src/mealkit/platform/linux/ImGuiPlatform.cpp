// Dear ImGui platform glue for the X11 window (the Linux counterpart of imgui_impl_win32):
// mouse, keys, text, cursor shapes, focus, the CLIPBOARD selection and the Xft.dpi scale.
#include <algorithm>
#include <cfloat>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>

// X11 headers define macros (Bool, Status, None, Success): standard headers go first
#include "mealkit/platform/ImGuiPlatform.h"

#include "mealkit/platform/Window.h"

#include <imgui.h>

#include "X11Keys.h"

#include <X11/Xatom.h>
#include <X11/Xresource.h>
#include <X11/cursorfont.h>

namespace mealkit {
namespace {

struct Platform {
    Window* window = nullptr;
    Display* display = nullptr;
    ::Window xwindow = 0;
    std::chrono::steady_clock::time_point last;
    Cursor cursors[ImGuiMouseCursor_COUNT] = {};
    Cursor blank = 0;
    ImGuiMouseCursor shown = ImGuiMouseCursor_COUNT;
    Atom clipboard = 0, targets = 0, utf8 = 0, property = 0;
    std::string owned;    // text we serve while owning CLIPBOARD
    std::string received; // last text read from another program
};

Platform* g = nullptr;

ImGuiKey imguiKey(u8 vk) {
    if (vk >= 'A' && vk <= 'Z') return static_cast<ImGuiKey>(ImGuiKey_A + (vk - 'A'));
    if (vk >= '0' && vk <= '9') return static_cast<ImGuiKey>(ImGuiKey_0 + (vk - '0'));
    if (vk >= 0x70 && vk <= 0x7B) return static_cast<ImGuiKey>(ImGuiKey_F1 + (vk - 0x70));
    if (vk >= 0x60 && vk <= 0x69) return static_cast<ImGuiKey>(ImGuiKey_Keypad0 + (vk - 0x60));
    switch (vk) {
    case 0x08: return ImGuiKey_Backspace;
    case 0x09: return ImGuiKey_Tab;
    case 0x0D: return ImGuiKey_Enter;
    case 0x13: return ImGuiKey_Pause;
    case 0x14: return ImGuiKey_CapsLock;
    case 0x1B: return ImGuiKey_Escape;
    case 0x20: return ImGuiKey_Space;
    case 0x21: return ImGuiKey_PageUp;
    case 0x22: return ImGuiKey_PageDown;
    case 0x23: return ImGuiKey_End;
    case 0x24: return ImGuiKey_Home;
    case 0x25: return ImGuiKey_LeftArrow;
    case 0x26: return ImGuiKey_UpArrow;
    case 0x27: return ImGuiKey_RightArrow;
    case 0x28: return ImGuiKey_DownArrow;
    case 0x2C: return ImGuiKey_PrintScreen;
    case 0x2D: return ImGuiKey_Insert;
    case 0x2E: return ImGuiKey_Delete;
    case 0x5D: return ImGuiKey_Menu;
    case 0x6A: return ImGuiKey_KeypadMultiply;
    case 0x6B: return ImGuiKey_KeypadAdd;
    case 0x6D: return ImGuiKey_KeypadSubtract;
    case 0x6E: return ImGuiKey_KeypadDecimal;
    case 0x6F: return ImGuiKey_KeypadDivide;
    case 0x90: return ImGuiKey_NumLock;
    case 0x91: return ImGuiKey_ScrollLock;
    case 0xBA: return ImGuiKey_Semicolon;
    case 0xBB: return ImGuiKey_Equal;
    case 0xBC: return ImGuiKey_Comma;
    case 0xBD: return ImGuiKey_Minus;
    case 0xBE: return ImGuiKey_Period;
    case 0xBF: return ImGuiKey_Slash;
    case 0xC0: return ImGuiKey_GraveAccent;
    case 0xDB: return ImGuiKey_LeftBracket;
    case 0xDC: return ImGuiKey_Backslash;
    case 0xDD: return ImGuiKey_RightBracket;
    case 0xDE: return ImGuiKey_Apostrophe;
    default: return ImGuiKey_None;
    }
}

// Left/right modifier keys come from the symbol itself (vkFromKeysym folds them)
ImGuiKey modifierKey(KeySym sym) {
    switch (sym) {
    case XK_Shift_L: return ImGuiKey_LeftShift;
    case XK_Shift_R: return ImGuiKey_RightShift;
    case XK_Control_L: return ImGuiKey_LeftCtrl;
    case XK_Control_R: return ImGuiKey_RightCtrl;
    case XK_Alt_L:
    case XK_Meta_L: return ImGuiKey_LeftAlt;
    case XK_Alt_R:
    case XK_Meta_R: return ImGuiKey_RightAlt;
    case XK_Super_L: return ImGuiKey_LeftSuper;
    case XK_Super_R: return ImGuiKey_RightSuper;
    default: return ImGuiKey_None;
    }
}

void updateModifiers(ImGuiIO& io, unsigned state) {
    io.AddKeyEvent(ImGuiMod_Ctrl, (state & ControlMask) != 0);
    io.AddKeyEvent(ImGuiMod_Shift, (state & ShiftMask) != 0);
    io.AddKeyEvent(ImGuiMod_Alt, (state & Mod1Mask) != 0);
    io.AddKeyEvent(ImGuiMod_Super, (state & Mod4Mask) != 0);
}

// Serves CLIPBOARD requests from other programs while we own it
void answerSelection(const XSelectionRequestEvent& req) {
    XSelectionEvent reply{};
    reply.type = SelectionNotify;
    reply.display = req.display;
    reply.requestor = req.requestor;
    reply.selection = req.selection;
    reply.target = req.target;
    reply.time = req.time;
    reply.property = None;
    if (req.target == g->targets) {
        const Atom supported[] = {g->targets, g->utf8, XA_STRING};
        XChangeProperty(g->display, req.requestor, req.property, XA_ATOM, 32, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(supported), 3);
        reply.property = req.property;
    } else if (req.target == g->utf8 || req.target == XA_STRING) {
        XChangeProperty(g->display, req.requestor, req.property, req.target, 8, PropModeReplace,
                        reinterpret_cast<const unsigned char*>(g->owned.data()), static_cast<int>(g->owned.size()));
        reply.property = req.property;
    }
    XSendEvent(g->display, req.requestor, False, 0, reinterpret_cast<XEvent*>(&reply));
    XFlush(g->display);
}

const char* getClipboard(ImGuiContext*) {
    if (XGetSelectionOwner(g->display, g->clipboard) == g->xwindow) return g->owned.c_str();
    g->received.clear();
    if (XGetSelectionOwner(g->display, g->clipboard) == None) return g->received.c_str();
    XConvertSelection(g->display, g->clipboard, g->utf8, g->property, g->xwindow, CurrentTime);
    XFlush(g->display);
    // Wait up to 250 ms for the owner's answer (only our SelectionNotify is taken out of the queue)
    const auto until = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
    XEvent event;
    bool answered = false;
    while (std::chrono::steady_clock::now() < until) {
        if (XCheckTypedWindowEvent(g->display, g->xwindow, SelectionNotify, &event)) {
            answered = true;
            break;
        }
        struct timespec ts{0, 2'000'000};
        nanosleep(&ts, nullptr);
    }
    if (!answered || event.xselection.property == None) return g->received.c_str();
    Atom type = 0;
    int format = 0;
    unsigned long count = 0, after = 0;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(g->display, g->xwindow, g->property, 0, 1 << 24, True, AnyPropertyType, &type, &format,
                           &count, &after, &data) == Success &&
        data) {
        if (format == 8) g->received.assign(reinterpret_cast<const char*>(data), count);
        XFree(data);
    }
    return g->received.c_str();
}

void setClipboard(ImGuiContext*, const char* text) {
    g->owned = text ? text : "";
    XSetSelectionOwner(g->display, g->clipboard, g->xwindow, CurrentTime);
    XFlush(g->display);
}

bool openInShell(ImGuiContext*, const char* path) {
    const std::string command = std::string("xdg-open '") + path + "' >/dev/null 2>&1 &";
    return std::system(command.c_str()) == 0;
}

std::intptr_t messageHook(void*, void* eventPtr, u32, std::uintptr_t, std::intptr_t) {
    if (!g) return 0;
    XEvent& event = *static_cast<XEvent*>(eventPtr);
    ImGuiIO& io = ImGui::GetIO();
    switch (event.type) {
    case MotionNotify: io.AddMousePosEvent(static_cast<float>(event.xmotion.x), static_cast<float>(event.xmotion.y)); break;
    case LeaveNotify:
        if (event.xcrossing.mode == NotifyNormal) io.AddMousePosEvent(-FLT_MAX, -FLT_MAX);
        break;
    case ButtonPress:
    case ButtonRelease: {
        const bool down = event.type == ButtonPress;
        io.AddMousePosEvent(static_cast<float>(event.xbutton.x), static_cast<float>(event.xbutton.y));
        switch (event.xbutton.button) {
        case Button1: io.AddMouseButtonEvent(ImGuiMouseButton_Left, down); break;
        case Button2: io.AddMouseButtonEvent(ImGuiMouseButton_Middle, down); break;
        case Button3: io.AddMouseButtonEvent(ImGuiMouseButton_Right, down); break;
        case Button4:
            if (down) io.AddMouseWheelEvent(0.0f, 1.0f);
            break;
        case Button5:
            if (down) io.AddMouseWheelEvent(0.0f, -1.0f);
            break;
        case 6:
            if (down) io.AddMouseWheelEvent(1.0f, 0.0f);
            break;
        case 7:
            if (down) io.AddMouseWheelEvent(-1.0f, 0.0f);
            break;
        case 8: io.AddMouseButtonEvent(3, down); break; // back / forward
        case 9: io.AddMouseButtonEvent(4, down); break;
        default: break;
        }
        break;
    }
    case KeyPress:
    case KeyRelease: {
        const bool down = event.type == KeyPress;
        updateModifiers(io, event.xkey.state);
        const KeySym sym = XkbKeycodeToKeysym(g->display, static_cast<KeyCode>(event.xkey.keycode), 0, 0);
        ImGuiKey key = modifierKey(sym);
        if (key == ImGuiKey_None) key = imguiKey(x11::vkFromKeysym(sym));
        if (key != ImGuiKey_None) io.AddKeyEvent(key, down);
        break;
    }
    case FocusIn: io.AddFocusEvent(true); break;
    case FocusOut:
        if (event.xfocus.mode != NotifyGrab && event.xfocus.mode != NotifyUngrab) io.AddFocusEvent(false);
        break;
    case SelectionRequest: answerSelection(event.xselectionrequest); return 1;
    case SelectionClear: g->owned.clear(); return 1;
    default: break;
    }
    return 0; // the window keeps its own input state too
}

void updateCursor() {
    ImGuiIO& io = ImGui::GetIO();
    if (io.ConfigFlags & ImGuiConfigFlags_NoMouseCursorChange) return;
    const ImGuiMouseCursor wanted = io.MouseDrawCursor ? ImGuiMouseCursor_None : ImGui::GetMouseCursor();
    if (wanted == g->shown) return;
    g->shown = wanted;
    const Cursor cursor = wanted == ImGuiMouseCursor_None ? g->blank
                          : g->cursors[wanted]            ? g->cursors[wanted]
                                                          : g->cursors[ImGuiMouseCursor_Arrow];
    XDefineCursor(g->display, g->xwindow, cursor);
}

} // namespace

bool imguiPlatformInit(Window& window) {
    const auto* handle = static_cast<const XlibWindowHandle*>(window.nativeHandle());
    if (!handle || !handle->display) return false;
    g = new Platform;
    g->window = &window;
    g->display = static_cast<Display*>(handle->display);
    g->xwindow = static_cast<::Window>(handle->window);
    g->last = std::chrono::steady_clock::now();
    g->clipboard = XInternAtom(g->display, "CLIPBOARD", False);
    g->targets = XInternAtom(g->display, "TARGETS", False);
    g->utf8 = XInternAtom(g->display, "UTF8_STRING", False);
    g->property = XInternAtom(g->display, "MEALKIT_CLIPBOARD", False);
    const unsigned shapes[ImGuiMouseCursor_COUNT] = {
        XC_left_ptr,           // Arrow
        XC_xterm,              // TextInput
        XC_fleur,              // ResizeAll
        XC_sb_v_double_arrow,  // ResizeNS
        XC_sb_h_double_arrow,  // ResizeEW
        XC_bottom_left_corner, // ResizeNESW
        XC_bottom_right_corner,// ResizeNWSE
        XC_hand2,              // Hand
        XC_watch,              // Wait
        XC_watch,              // Progress
        XC_X_cursor,           // NotAllowed
    };
    for (int i = 0; i < ImGuiMouseCursor_COUNT; ++i) g->cursors[i] = XCreateFontCursor(g->display, shapes[i]);
    char empty[1] = {0};
    Pixmap pixmap = XCreateBitmapFromData(g->display, g->xwindow, empty, 1, 1);
    XColor black{};
    g->blank = XCreatePixmapCursor(g->display, pixmap, pixmap, &black, &black, 0, 0);
    XFreePixmap(g->display, pixmap);

    ImGuiIO& io = ImGui::GetIO();
    io.BackendPlatformName = "mealkit_x11";
    io.BackendFlags |= ImGuiBackendFlags_HasMouseCursors;
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    pio.Platform_GetClipboardTextFn = getClipboard;
    pio.Platform_SetClipboardTextFn = setClipboard;
    pio.Platform_OpenInShellFn = openInShell;
    window.setMessageHook(messageHook, nullptr);
    return true;
}

void imguiPlatformShutdown(Window& window) {
    window.setMessageHook(nullptr, nullptr);
    if (!g) return;
    for (Cursor c : g->cursors)
        if (c) XFreeCursor(g->display, c);
    if (g->blank) XFreeCursor(g->display, g->blank);
    ImGuiIO& io = ImGui::GetIO();
    io.BackendPlatformName = nullptr;
    io.BackendFlags &= ~ImGuiBackendFlags_HasMouseCursors;
    ImGuiPlatformIO& pio = ImGui::GetPlatformIO();
    pio.Platform_GetClipboardTextFn = nullptr;
    pio.Platform_SetClipboardTextFn = nullptr;
    pio.Platform_OpenInShellFn = nullptr;
    delete g;
    g = nullptr;
}

void imguiPlatformNewFrame() {
    if (!g) return;
    ImGuiIO& io = ImGui::GetIO();
    io.DisplaySize = ImVec2(static_cast<float>(g->window->width()), static_cast<float>(g->window->height()));
    const auto now = std::chrono::steady_clock::now();
    io.DeltaTime = std::max(1e-6f, std::chrono::duration<float>(now - g->last).count());
    g->last = now;
    // Text already went through the input method in Window::pumpMessages (one lookup per key event)
    const std::string& text = g->window->input().text();
    if (!text.empty()) io.AddInputCharactersUTF8(text.c_str());
    updateCursor();
}

f32 imguiDpiScale(Window& window) {
    const auto* handle = static_cast<const XlibWindowHandle*>(window.nativeHandle());
    if (!handle || !handle->display) return 1.0f;
    // Xft.dpi is what GNOME/KDE set for their scale factor (XWayland included); 96 = 1.0
    const char* resources = XResourceManagerString(static_cast<Display*>(handle->display));
    if (!resources) return 1.0f;
    XrmInitialize();
    XrmDatabase db = XrmGetStringDatabase(resources);
    f32 scale = 1.0f;
    char* type = nullptr;
    XrmValue value{};
    if (db && XrmGetResource(db, "Xft.dpi", "Xft.Dpi", &type, &value) && value.addr) {
        const f32 dpi = static_cast<f32>(std::atof(value.addr));
        if (dpi > 0.0f) scale = dpi / 96.0f;
    }
    if (db) XrmDestroyDatabase(db);
    return scale;
}

} // namespace mealkit
