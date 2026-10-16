#pragma once

#include "mealkit/core/Types.h"
#include "mealkit/platform/Input.h"

#include <cstdint>

namespace mealkit {

constexpr u32 kDefaultCaptionColor = 0xFFFFFFFFu;

struct WindowDesc {
    const char* title = "MealKit";
    u32 width = 1280; // 클라이언트 영역 크기(물리 픽셀)
    u32 height = 720;
    // 제목 표시줄 색(0xRRGGBB). 지정하면 어두운 제목 표시줄(흰 글자·버튼)을 켜고, Windows 11에서는 이 색으로 칠한다
    u32 captionColor = kDefaultCaptionColor;
};

// 다른 모듈(에디터 UI 등)이 창 메시지를 먼저 받아 보는 훅. 0이 아닌 값을 반환하면 그 메시지는 처리 완료로 본다.
using MessageHook = std::intptr_t (*)(void* user, void* hwnd, u32 msg, std::uintptr_t wparam, std::intptr_t lparam);

class Window {
public:
    Window() = default;
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;

    bool create(const WindowDesc& desc);

    // 메시지를 처리하고 입력 상태를 갱신한다. 창을 닫으면 false.
    bool pumpMessages();

    void* nativeHandle() const { return m_hwnd; } // HWND
    u32 width() const { return m_width; }
    u32 height() const { return m_height; }
    bool minimized() const { return m_minimized; }
    bool focused() const { return m_focused; }
    bool consumeResize(); // 크기가 바뀐 뒤 한 번만 true

    void setTitle(const char* utf8);
    void setCursorCaptured(bool captured); // 커서 숨김 + 창 안에 가둠 (카메라 회전용)

    Input& input() { return m_input; }
    void setMessageHook(MessageHook hook, void* user);

    // WndProc에서 호출한다.
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
    Input m_input;
    MessageHook m_hook = nullptr;
    void* m_hookUser = nullptr;
};

} // namespace mealkit
