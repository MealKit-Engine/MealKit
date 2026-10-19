#include "mealkit/platform/ImGuiPlatform.h"

#include "mealkit/platform/Window.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <windows.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

namespace mealkit {
namespace {
std::intptr_t messageHook(void*, void* hwnd, u32 msg, std::uintptr_t wparam, std::intptr_t lparam) {
    return ImGui_ImplWin32_WndProcHandler(static_cast<HWND>(hwnd), msg, wparam, lparam);
}
} // namespace

bool imguiPlatformInit(Window& window) {
    if (!ImGui_ImplWin32_Init(window.nativeHandle())) return false;
    window.setMessageHook(messageHook, nullptr);
    return true;
}

void imguiPlatformShutdown(Window& window) {
    ImGui_ImplWin32_Shutdown();
    window.setMessageHook(nullptr, nullptr);
}

void imguiPlatformNewFrame() { ImGui_ImplWin32_NewFrame(); }

f32 imguiDpiScale(Window& window) { return ImGui_ImplWin32_GetDpiScaleForHwnd(window.nativeHandle()); }

} // namespace mealkit
