// Linux window: X11 (Xlib), also on Wayland desktops through XWayland (docs/DECISIONS.md 2026-11-22).
// Raw mouse movement comes from XInput2 raw motion, text from the X input method (XIM: ibus/fcitx Korean input).
#include <algorithm>
#include <clocale>
#include <cstdlib>
#include <cstring>
#include <string>

// X11 headers define macros (Bool, Status, None, Success): standard headers go first
#include "mealkit/platform/Window.h"

#include "mealkit/core/Log.h"
#include "X11Keys.h"

#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/extensions/XInput2.h>
#include <X11/extensions/Xrandr.h>

namespace mealkit {
namespace {

struct X11State {
    XlibWindowHandle handle; // what nativeHandle() points at
    Display* display = nullptr;
    ::Window window = 0;
    XIM im = nullptr;
    XIC ic = nullptr;
    int xiOpcode = -1; // XInput2 extension (raw motion), -1 if missing
    Cursor blankCursor = 0;
    Atom wmDelete = 0, wmProtocols = 0, netWmState = 0, netWmStateFullscreen = 0, netWmStateMaxVert = 0,
         netWmStateMaxHorz = 0, netWmName = 0, utf8String = 0;
};

X11State* state(void* p) { return static_cast<X11State*>(p); }

// _NET_WM_STATE change request through the window manager (add 1, remove 0)
void sendWmState(X11State& x, bool add, Atom a, Atom b) {
    XEvent e{};
    e.xclient.type = ClientMessage;
    e.xclient.window = x.window;
    e.xclient.message_type = x.netWmState;
    e.xclient.format = 32;
    e.xclient.data.l[0] = add ? 1 : 0;
    e.xclient.data.l[1] = static_cast<long>(a);
    e.xclient.data.l[2] = static_cast<long>(b);
    e.xclient.data.l[3] = 1; // normal application
    XSendEvent(x.display, DefaultRootWindow(x.display), False, SubstructureRedirectMask | SubstructureNotifyMask, &e);
}

bool hasWmState(X11State& x, Atom wanted) {
    Atom type = 0;
    int format = 0;
    unsigned long count = 0, after = 0;
    unsigned char* data = nullptr;
    if (XGetWindowProperty(x.display, x.window, x.netWmState, 0, 64, False, XA_ATOM, &type, &format, &count, &after,
                           &data) != Success)
        return false;
    bool found = false;
    const Atom* atoms = reinterpret_cast<const Atom*>(data);
    for (unsigned long i = 0; i < count; ++i) found = found || atoms[i] == wanted;
    if (data) XFree(data);
    return found;
}

// Decodes one UTF-8 string into code points for Input::addText
void addUtf8(Input& input, const char* s, int len) {
    const auto* p = reinterpret_cast<const unsigned char*>(s);
    for (int i = 0; i < len;) {
        u32 c = p[i];
        int extra = c >= 0xF0 ? 3 : c >= 0xE0 ? 2 : c >= 0xC0 ? 1 : 0;
        if (extra) c &= 0x3F >> extra;
        ++i;
        for (; extra > 0 && i < len; --extra, ++i) c = (c << 6) | (p[i] & 0x3F);
        input.addText(c);
    }
}

} // namespace

Window::~Window() {
    setCursorCaptured(false);
    X11State* x = state(m_platform);
    if (!x) return;
    if (x->ic) XDestroyIC(x->ic);
    if (x->im) XCloseIM(x->im);
    if (x->blankCursor) XFreeCursor(x->display, x->blankCursor);
    if (x->window) XDestroyWindow(x->display, x->window);
    XCloseDisplay(x->display);
    delete x;
}

bool Window::create(const WindowDesc& desc) {
    std::setlocale(LC_CTYPE, ""); // XIM and Xutf8LookupString follow the locale
    XSetLocaleModifiers("");
    Display* display = XOpenDisplay(nullptr);
    if (!display) {
        MEALKIT_ERROR("Can't connect to the X server (DISPLAY='%s'). On Wayland, XWayland is needed.",
                      std::getenv("DISPLAY") ? std::getenv("DISPLAY") : "");
        return false;
    }
    auto* x = new X11State;
    x->display = display;
    m_platform = x;

    const int screen = DefaultScreen(display);
    XSetWindowAttributes attributes{};
    attributes.background_pixel = BlackPixel(display, screen);
    attributes.event_mask = KeyPressMask | KeyReleaseMask | ButtonPressMask | ButtonReleaseMask | PointerMotionMask |
                            StructureNotifyMask | FocusChangeMask | ExposureMask | EnterWindowMask | LeaveWindowMask;
    x->window = XCreateWindow(display, RootWindow(display, screen), 0, 0, desc.width, desc.height, 0,
                              CopyFromParent, InputOutput, CopyFromParent, CWBackPixel | CWEventMask, &attributes);
    if (!x->window) {
        MEALKIT_ERROR("XCreateWindow failed");
        return false;
    }
    x->handle = {display, x->window};
    m_hwnd = &x->handle;

    x->wmProtocols = XInternAtom(display, "WM_PROTOCOLS", False);
    x->wmDelete = XInternAtom(display, "WM_DELETE_WINDOW", False);
    x->netWmState = XInternAtom(display, "_NET_WM_STATE", False);
    x->netWmStateFullscreen = XInternAtom(display, "_NET_WM_STATE_FULLSCREEN", False);
    x->netWmStateMaxVert = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_VERT", False);
    x->netWmStateMaxHorz = XInternAtom(display, "_NET_WM_STATE_MAXIMIZED_HORZ", False);
    x->netWmName = XInternAtom(display, "_NET_WM_NAME", False);
    x->utf8String = XInternAtom(display, "UTF8_STRING", False);
    XSetWMProtocols(display, x->window, &x->wmDelete, 1);

    XClassHint hint{};
    char resName[] = "mealkit", resClass[] = "MealKit";
    hint.res_name = resName;
    hint.res_class = resClass;
    XSetClassHint(display, x->window, &hint);
    if (desc.captionColor != kDefaultCaptionColor) {
        // Dark title bar on GTK window managers (GNOME/mutter); the exact color isn't settable on X11
        const char dark[] = "dark";
        XChangeProperty(display, x->window, XInternAtom(display, "_GTK_THEME_VARIANT", False), x->utf8String, 8,
                        PropModeReplace, reinterpret_cast<const unsigned char*>(dark), 4);
    }
    setTitle(desc.title);

    // Key repeat sends only presses (no fake releases), like WM_KEYDOWN repeats
    Bool supported = False;
    XkbSetDetectableAutoRepeat(display, True, &supported);

    // Input method: preedit drawn by the IME itself (ibus/fcitx candidate window)
    x->im = XOpenIM(display, nullptr, nullptr, nullptr);
    if (x->im) {
        x->ic = XCreateIC(x->im, XNInputStyle, XIMPreeditNothing | XIMStatusNothing, XNClientWindow, x->window,
                          XNFocusWindow, x->window, nullptr);
        if (x->ic) {
            long imEvents = 0;
            XGetICValues(x->ic, XNFilterEvents, &imEvents, nullptr);
            XSelectInput(display, x->window, attributes.event_mask | imEvents);
            XSetICFocus(x->ic);
        }
    }
    if (!x->ic) MEALKIT_WARN("No X input method: text input without IME composition");

    // XInput2 raw motion: movement without acceleration, also while the pointer is confined
    int event = 0, error = 0, major = 2, minor = 0;
    if (XQueryExtension(display, "XInputExtension", &x->xiOpcode, &event, &error) &&
        XIQueryVersion(display, &major, &minor) == Success) {
        unsigned char mask[XIMaskLen(XI_RawMotion)] = {};
        XISetMask(mask, XI_RawMotion);
        XIEventMask em{XIAllMasterDevices, sizeof(mask), mask};
        XISelectEvents(display, DefaultRootWindow(display), &em, 1);
    } else {
        x->xiOpcode = -1;
        MEALKIT_WARN("Raw mouse input unavailable (XInput2 missing)");
    }

    char empty[1] = {0};
    Pixmap pixmap = XCreateBitmapFromData(display, x->window, empty, 1, 1);
    XColor black{};
    x->blankCursor = XCreatePixmapCursor(display, pixmap, pixmap, &black, &black, 0, 0);
    XFreePixmap(display, pixmap);

    XMapWindow(display, x->window);
    XFlush(display);
    m_width = desc.width;
    m_height = desc.height;
    return true;
}

void Window::setMode(WindowMode mode, u32 width, u32 height) {
    X11State* x = state(m_platform);
    if (!x) return;
    m_mode = mode;
    // Borderless and Fullscreen are both the window manager's fullscreen state (a borderless window over the monitor)
    sendWmState(*x, mode != WindowMode::Windowed, x->netWmStateFullscreen, 0);
    if (mode == WindowMode::Windowed) {
        sendWmState(*x, false, x->netWmStateMaxVert, x->netWmStateMaxHorz);
        const Screen* screen = DefaultScreenOfDisplay(x->display);
        const int left = std::max(0, (WidthOfScreen(screen) - static_cast<int>(width)) / 2);
        const int top = std::max(0, (HeightOfScreen(screen) - static_cast<int>(height)) / 2);
        XMoveResizeWindow(x->display, x->window, left, top, width, height);
    }
    XFlush(x->display);
}

std::vector<DisplaySize> Window::displaySizes() const {
    std::vector<DisplaySize> out;
    X11State* x = state(m_platform);
    if (!x) return out;
    XRRScreenResources* res = XRRGetScreenResourcesCurrent(x->display, DefaultRootWindow(x->display));
    if (!res) return out;
    for (int i = 0; i < res->nmode; ++i) {
        const DisplaySize s{res->modes[i].width, res->modes[i].height};
        if (s.width < 800 || s.height < 600) continue;
        bool seen = false;
        for (const DisplaySize& o : out) seen = seen || (o.width == s.width && o.height == s.height);
        if (!seen) out.push_back(s);
    }
    XRRFreeScreenResources(res);
    std::sort(out.begin(), out.end(), [](const DisplaySize& a, const DisplaySize& b) {
        return a.width * a.height > b.width * b.height;
    });
    return out;
}

WindowPlacement Window::placement() const {
    WindowPlacement out;
    X11State* x = state(m_platform);
    if (!x) return out;
    ::Window child = 0;
    int left = 0, top = 0;
    XTranslateCoordinates(x->display, x->window, DefaultRootWindow(x->display), 0, 0, &left, &top, &child);
    out = {left, top, static_cast<i32>(m_width), static_cast<i32>(m_height),
           hasWmState(*x, x->netWmStateMaxVert) && hasWmState(*x, x->netWmStateMaxHorz)};
    return out;
}

void Window::setPlacement(const WindowPlacement& placement) {
    X11State* x = state(m_platform);
    if (!x || placement.width <= 0 || placement.height <= 0) return;
    // Keep the window on the screen if the saved monitor is gone
    const Screen* screen = DefaultScreenOfDisplay(x->display);
    const int w = std::min(placement.width, WidthOfScreen(screen));
    const int h = std::min(placement.height, HeightOfScreen(screen));
    const int left = std::clamp(placement.x, 0, std::max(0, WidthOfScreen(screen) - w));
    const int top = std::clamp(placement.y, 0, std::max(0, HeightOfScreen(screen) - h));
    XMoveResizeWindow(x->display, x->window, left, top, static_cast<unsigned>(w), static_cast<unsigned>(h));
    if (placement.maximized) sendWmState(*x, true, x->netWmStateMaxVert, x->netWmStateMaxHorz);
    XFlush(x->display);
}

bool Window::pumpMessages() {
    m_input.beginFrame();
    X11State* x = state(m_platform);
    if (!x) return !m_quit;
    while (XPending(x->display)) {
        XEvent event;
        XNextEvent(x->display, &event);
        if (XFilterEvent(&event, None)) continue; // consumed by the input method (composition)
        handleMessage(&event, static_cast<u32>(event.type), 0, 0);
    }
    return !m_quit;
}

void Window::close() { m_quit = true; }

bool Window::consumeResize() {
    const bool r = m_resized;
    m_resized = false;
    return r;
}

void Window::setTitle(const char* utf8) {
    X11State* x = state(m_platform);
    if (!x) return;
    const auto* text = reinterpret_cast<const unsigned char*>(utf8);
    const int len = static_cast<int>(std::strlen(utf8));
    XChangeProperty(x->display, x->window, x->netWmName, x->utf8String, 8, PropModeReplace, text, len);
    XStoreName(x->display, x->window, utf8); // WM_NAME for window managers without EWMH
    XFlush(x->display);
}

void Window::setCursorCaptured(bool captured) {
    X11State* x = state(m_platform);
    if (captured == m_captured || !x) return;
    m_captured = captured;
    if (captured) {
        XGrabPointer(x->display, x->window, True, ButtonPressMask | ButtonReleaseMask | PointerMotionMask,
                     GrabModeAsync, GrabModeAsync, x->window, x->blankCursor, CurrentTime);
    } else {
        XUngrabPointer(x->display, CurrentTime);
    }
    XFlush(x->display);
}

void Window::setMessageHook(MessageHook hook, void* user) {
    m_hook = hook;
    m_hookUser = user;
}

std::intptr_t Window::handleMessage(void* eventPtr, u32 type, std::uintptr_t wparam, std::intptr_t lparam) {
    X11State* x = state(m_platform);
    XEvent& event = *static_cast<XEvent*>(eventPtr);
    if (m_hook) {
        const std::intptr_t handled = m_hook(m_hookUser, eventPtr, type, wparam, lparam);
        if (handled) return handled;
    }
    switch (event.type) {
    case ClientMessage:
        if (event.xclient.message_type == x->wmProtocols && static_cast<Atom>(event.xclient.data.l[0]) == x->wmDelete)
            m_quit = true;
        break;
    case ConfigureNotify: {
        const u32 w = static_cast<u32>(event.xconfigure.width), h = static_cast<u32>(event.xconfigure.height);
        if (w != m_width || h != m_height) {
            m_width = w;
            m_height = h;
            m_resized = true;
        }
        break;
    }
    case MapNotify: m_minimized = false; break;
    case UnmapNotify: m_minimized = true; break;
    case FocusIn:
        m_focused = true;
        if (x->ic) XSetICFocus(x->ic);
        break;
    case FocusOut:
        if (event.xfocus.mode == NotifyGrab || event.xfocus.mode == NotifyUngrab) break; // our own pointer grab
        m_focused = false;
        m_input.clear();
        setCursorCaptured(false);
        if (x->ic) XUnsetICFocus(x->ic);
        break;
    case KeyPress: {
        if (const u8 vk = x11::vkFromEvent(x->display, event.xkey)) m_input.setKey(vk, true);
        char buffer[64];
        KeySym sym = 0;
        int len = 0;
        if (x->ic) {
            Status status = 0;
            len = Xutf8LookupString(x->ic, &event.xkey, buffer, sizeof(buffer), &sym, &status);
            if (status != XLookupChars && status != XLookupBoth) len = 0;
        } else {
            len = XLookupString(&event.xkey, buffer, sizeof(buffer), &sym, nullptr);
        }
        // Ctrl/Alt shortcuts aren't text (WM_CHAR gives control characters there, which addText drops too)
        if (len > 0 && !(event.xkey.state & (ControlMask | Mod1Mask))) addUtf8(m_input, buffer, len);
        else if (len > 0 && buffer[0] == 8) m_input.addText(8);
        break;
    }
    case KeyRelease:
        if (const u8 vk = x11::vkFromEvent(x->display, event.xkey)) m_input.setKey(vk, false);
        break;
    case ButtonPress:
    case ButtonRelease: {
        const bool down = event.type == ButtonPress;
        switch (event.xbutton.button) {
        case Button1: m_input.setMouse(MouseButton::Left, down); break;
        case Button2: m_input.setMouse(MouseButton::Middle, down); break;
        case Button3: m_input.setMouse(MouseButton::Right, down); break;
        case Button4:
            if (down) m_input.addWheel(1.0f);
            break;
        case Button5:
            if (down) m_input.addWheel(-1.0f);
            break;
        default: break;
        }
        m_input.setMousePosition(static_cast<f32>(event.xbutton.x), static_cast<f32>(event.xbutton.y));
        break;
    }
    case MotionNotify:
        m_input.setMousePosition(static_cast<f32>(event.xmotion.x), static_cast<f32>(event.xmotion.y));
        break;
    case GenericEvent:
        if (event.xcookie.extension == x->xiOpcode && XGetEventData(x->display, &event.xcookie)) {
            if (event.xcookie.evtype == XI_RawMotion && m_focused) {
                const auto* raw = static_cast<const XIRawEvent*>(event.xcookie.data);
                const double* values = raw->raw_values;
                f32 dx = 0.0f, dy = 0.0f;
                if (XIMaskIsSet(raw->valuators.mask, 0)) dx = static_cast<f32>(*values++);
                if (XIMaskIsSet(raw->valuators.mask, 1)) dy = static_cast<f32>(*values);
                m_input.addMouseDelta(dx, dy);
            }
            XFreeEventData(x->display, &event.xcookie);
        }
        break;
    default: break;
    }
    return 0;
}

} // namespace mealkit
