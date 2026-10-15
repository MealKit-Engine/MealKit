#include "mealkit/platform/Window.h"

#include "mealkit/core/Log.h"

#include <string>
#include <windows.h>

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
    wc.hIcon = LoadIconW(nullptr, IDI_APPLICATION);
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
    case WM_CLOSE:
        m_quit = true;
        return 0;
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
    case WM_KEYDOWN:
        m_input.setKey(static_cast<u8>(wparam), true);
        return 0;
    case WM_KEYUP:
        m_input.setKey(static_cast<u8>(wparam), false);
        return 0;
    case WM_SYSKEYDOWN:
        m_input.setKey(static_cast<u8>(wparam), true);
        break; // Alt+F4 등은 기본 처리
    case WM_SYSKEYUP:
        m_input.setKey(static_cast<u8>(wparam), false);
        break;
    case WM_LBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_MBUTTONDOWN: {
        const MouseButton b = msg == WM_LBUTTONDOWN ? MouseButton::Left
                              : msg == WM_RBUTTONDOWN ? MouseButton::Right
                                                      : MouseButton::Middle;
        m_input.setMouse(b, true);
        SetCapture(hwnd);
        return 0;
    }
    case WM_LBUTTONUP:
    case WM_RBUTTONUP:
    case WM_MBUTTONUP: {
        const MouseButton b = msg == WM_LBUTTONUP ? MouseButton::Left
                              : msg == WM_RBUTTONUP ? MouseButton::Right
                                                    : MouseButton::Middle;
        m_input.setMouse(b, false);
        if (!m_input.mouseDown(MouseButton::Left) && !m_input.mouseDown(MouseButton::Right) &&
            !m_input.mouseDown(MouseButton::Middle))
            ReleaseCapture();
        return 0;
    }
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
    case WM_ERASEBKGND:
        return 1;
    default:
        break;
    }
    return DefWindowProcW(hwnd, msg, wparam, lparam);
}

} // namespace mealkit
