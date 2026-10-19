#include "mealkit/platform/Window.h"

#include "mealkit/core/Log.h"

#include <algorithm>
#include <string>
#include <windows.h>

#include <dwmapi.h>

namespace mealkit {
namespace {

constexpr wchar_t kClassName[] = L"MealKitWindow";

std::wstring widen(const char* utf8) {
    const int len = MultiByteToWideChar(CP_UTF8, 0, utf8, -1, nullptr, 0);
    std::wstring out(len > 0 ? len - 1 : 0, L'\0');
    if (len > 1) MultiByteToWideChar(CP_UTF8, 0, utf8, -1, out.data(), len);
    return out;
}

LRESULT CALLBACK windowProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam) {
    if (msg == WM_NCCREATE) {
        auto* cs = reinterpret_cast<CREATESTRUCTW*>(lparam);
        SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(cs->lpCreateParams));
    }
    auto* window = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
    if (window) return window->handleMessage(hwnd, msg, wparam, lparam);
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace

Window::~Window() {
    setCursorCaptured(false);
    if (m_hwnd) {
        SetWindowLongPtrW(static_cast<HWND>(m_hwnd), GWLP_USERDATA, 0);
        DestroyWindow(static_cast<HWND>(m_hwnd));
    }
}

bool Window::create(const WindowDesc& desc) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

    HINSTANCE instance = GetModuleHandleW(nullptr);
    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = windowProc;
    wc.hInstance = instance;
    wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
    // exe의 아이콘 리소스 1번 (없으면 Windows 기본 아이콘)
    wc.hIcon = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON, GetSystemMetrics(SM_CXICON),
                                             GetSystemMetrics(SM_CYICON), 0));
    wc.hIconSm = static_cast<HICON>(LoadImageW(instance, MAKEINTRESOURCEW(1), IMAGE_ICON,
                                               GetSystemMetrics(SM_CXSMICON), GetSystemMetrics(SM_CYSMICON), 0));
    if (!wc.hIcon) wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
    wc.lpszClassName = kClassName;
    if (!RegisterClassExW(&wc) && GetLastError() != ERROR_CLASS_ALREADY_EXISTS) {
        MEALKIT_ERROR("RegisterClassExW failed (%lu)", GetLastError());
        return false;
    }

    const DWORD style = WS_OVERLAPPEDWINDOW;
    RECT rect{0, 0, static_cast<LONG>(desc.width), static_cast<LONG>(desc.height)};
    AdjustWindowRectEx(&rect, style, FALSE, 0);

    HWND hwnd = CreateWindowExW(0, kClassName, widen(desc.title).c_str(), style, CW_USEDEFAULT, CW_USEDEFAULT,
                                rect.right - rect.left, rect.bottom - rect.top, nullptr, nullptr, instance, this);
    if (!hwnd) {
        MEALKIT_ERROR("CreateWindowExW failed (%lu)", GetLastError());
        return false;
    }
    m_hwnd = hwnd;

    if (desc.captionColor != kDefaultCaptionColor) {
        // 어두운 제목 표시줄(Windows 10 20H1+) + 제목 표시줄 색(Windows 11). 지원하지 않는 Windows에서는 조용히 무시된다
        const BOOL dark = TRUE;
        DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
        const u32 rgb = desc.captionColor;
        const COLORREF caption = RGB((rgb >> 16) & 0xFF, (rgb >> 8) & 0xFF, rgb & 0xFF);
        DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
    }

    RECT client;
    GetClientRect(hwnd, &client);
    m_width = static_cast<u32>(client.right - client.left);
    m_height = static_cast<u32>(client.bottom - client.top);

    // Raw Input 마우스: 커서 가속과 화면 경계의 영향을 받지 않는 이동량
    RAWINPUTDEVICE rid{};
    rid.usUsagePage = 0x01;
    rid.usUsage = 0x02;
    rid.hwndTarget = hwnd;
    if (!RegisterRawInputDevices(&rid, 1, sizeof(rid))) MEALKIT_WARN("Raw mouse input unavailable");

    ShowWindow(hwnd, SW_SHOW);
    return true;
}

void Window::setMode(WindowMode mode, u32 width, u32 height) {
    HWND hwnd = static_cast<HWND>(m_hwnd);
    if (!hwnd) return;
    m_mode = mode;
    MONITORINFO mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromWindow(hwnd, MONITOR_DEFAULTTONEAREST), &mi);
    if (mode == WindowMode::Windowed) {
        const DWORD style = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
        SetWindowLongPtrW(hwnd, GWL_STYLE, static_cast<LONG_PTR>(style));
        RECT rect{0, 0, static_cast<LONG>(width), static_cast<LONG>(height)};
        AdjustWindowRectEx(&rect, style, FALSE, 0);
        const LONG w = rect.right - rect.left, h = rect.bottom - rect.top;
        const RECT& work = mi.rcWork; // 작업 영역 가운데
        SetWindowPos(hwnd, HWND_NOTOPMOST, work.left + std::max(0L, (work.right - work.left - w) / 2),
                     work.top + std::max(0L, (work.bottom - work.top - h) / 2), w, h, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    } else {
        SetWindowLongPtrW(hwnd, GWL_STYLE, static_cast<LONG_PTR>(WS_POPUP | WS_VISIBLE));
        const RECT& r = mi.rcMonitor;
        SetWindowPos(hwnd, HWND_TOP, r.left, r.top, r.right - r.left, r.bottom - r.top, SWP_FRAMECHANGED | SWP_SHOWWINDOW);
    }
}

std::vector<DisplaySize> Window::displaySizes() const {
    std::vector<DisplaySize> out;
    MONITORINFOEXW mi{};
    mi.cbSize = sizeof(mi);
    GetMonitorInfoW(MonitorFromWindow(static_cast<HWND>(m_hwnd), MONITOR_DEFAULTTONEAREST), &mi);
    DEVMODEW dm{};
    dm.dmSize = sizeof(dm);
    for (DWORD i = 0; EnumDisplaySettingsW(mi.szDevice, i, &dm); ++i) {
        const DisplaySize s{dm.dmPelsWidth, dm.dmPelsHeight};
        if (s.width < 800 || s.height < 600) continue;
        bool seen = false;
        for (const DisplaySize& o : out) seen = seen || (o.width == s.width && o.height == s.height);
        if (!seen) out.push_back(s);
    }
    std::sort(out.begin(), out.end(), [](const DisplaySize& a, const DisplaySize& b) {
        return a.width * a.height > b.width * b.height;
    });
    return out;
}

bool Window::pumpMessages() {
    m_input.beginFrame();
    MSG msg;
    while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE)) {
        if (msg.message == WM_QUIT) m_quit = true;
        TranslateMessage(&msg);
        DispatchMessageW(&msg);
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
    if (m_hwnd) SetWindowTextW(static_cast<HWND>(m_hwnd), widen(utf8).c_str());
}

void Window::setCursorCaptured(bool captured) {
    if (captured == m_captured || !m_hwnd) return;
    m_captured = captured;
    if (captured) {
        ShowCursor(FALSE);
        RECT r;
        GetClientRect(static_cast<HWND>(m_hwnd), &r);
        MapWindowPoints(static_cast<HWND>(m_hwnd), nullptr, reinterpret_cast<POINT*>(&r), 2);
        ClipCursor(&r);
    } else {
        ClipCursor(nullptr);
        ShowCursor(TRUE);
    }
}

void Window::setMessageHook(MessageHook hook, void* user) {
    m_hook = hook;
    m_hookUser = user;
}

std::intptr_t Window::handleMessage(void* hwndPtr, u32 msg, std::uintptr_t wparam, std::intptr_t lparam) {
    HWND hwnd = static_cast<HWND>(hwndPtr);
    if (m_hook) {
        const std::intptr_t handled = m_hook(m_hookUser, hwndPtr, msg, wparam, lparam);
        if (handled) return handled;
    }
    switch (msg) {
    case WM_CLOSE: m_quit = true; return 0;
    case WM_SIZE:
        m_minimized = wparam == SIZE_MINIMIZED;
        if (!m_minimized) {
            const u32 w = LOWORD(lparam), h = HIWORD(lparam);
            if (w != m_width || h != m_height) {
                m_width = w;
                m_height = h;
                m_resized = true;
            }
        }
        return 0;
    case WM_ACTIVATE:
        m_focused = LOWORD(wparam) != WA_INACTIVE;
        if (!m_focused) {
            m_input.clear();
            setCursorCaptured(false);
        }
        return 0;
    case WM_CHAR: { // 글자 입력 위젯 (UTF-16, 두 개로 오는 글자는 합친다)
        const u32 unit = static_cast<u32>(wparam);
        if (unit >= 0xD800 && unit < 0xDC00) {
            m_highSurrogate = unit;
        } else if (unit >= 0xDC00 && unit < 0xE000) {
            if (m_highSurrogate) m_input.addText(0x10000 + ((m_highSurrogate - 0xD800) << 10) + (unit - 0xDC00));
            m_highSurrogate = 0;
        } else {
            m_input.addText(unit);
        }
        return 0;
    }
    case WM_KEYDOWN: m_input.setKey(static_cast<u8>(wparam), true); return 0;
    case WM_KEYUP: m_input.setKey(static_cast<u8>(wparam), false); return 0;
    case WM_SYSKEYDOWN: m_input.setKey(static_cast<u8>(wparam), true); break; // Alt+F4 등은 기본 처리
    case WM_SYSKEYUP: m_input.setKey(static_cast<u8>(wparam), false); break;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN: {
        const MouseButton b = msg == WM_LBUTTONDOWN   ? MouseButton::Left
                              : msg == WM_RBUTTONDOWN ? MouseButton::Right
                                                      : MouseButton::Middle;
        m_input.setMouse(b, true);
        SetCapture(hwnd);
        return 0;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
        const MouseButton b = msg == WM_LBUTTONUP   ? MouseButton::Left
                              : msg == WM_RBUTTONUP ? MouseButton::Right
                                                    : MouseButton::Middle;
        m_input.setMouse(b, false);
        if (!m_input.mouseDown(MouseButton::Left) && !m_input.mouseDown(MouseButton::Right) &&
            !m_input.mouseDown(MouseButton::Middle))
            ReleaseCapture();
        return 0;
    }
    case WM_MOUSEMOVE:
        m_input.setMousePosition(static_cast<f32>(static_cast<i16>(LOWORD(lparam))),
                                 static_cast<f32>(static_cast<i16>(HIWORD(lparam))));
        return 0;
    case WM_MOUSEWHEEL:
        m_input.addWheel(static_cast<f32>(GET_WHEEL_DELTA_WPARAM(wparam)) / static_cast<f32>(WHEEL_DELTA));
        return 0;
    case WM_INPUT: {
        RAWINPUT raw;
        UINT size = sizeof(raw);
        if (GetRawInputData(reinterpret_cast<HRAWINPUT>(lparam), RID_INPUT, &raw, &size, sizeof(RAWINPUTHEADER)) !=
                static_cast<UINT>(-1) &&
            raw.header.dwType == RIM_TYPEMOUSE && !(raw.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE)) {
            m_input.addMouseDelta(static_cast<f32>(raw.data.mouse.lLastX), static_cast<f32>(raw.data.mouse.lLastY));
        }
        break; // WM_INPUT은 DefWindowProc이 정리해야 한다
    }
    case WM_DPICHANGED: {
        const RECT* r = reinterpret_cast<const RECT*>(lparam);
        SetWindowPos(hwnd, nullptr, r->left, r->top, r->right - r->left, r->bottom - r->top,
                     SWP_NOZORDER | SWP_NOACTIVATE);
        return 0;
    }
    case WM_ERASEBKGND: return 1;
    default: break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace mealkit
