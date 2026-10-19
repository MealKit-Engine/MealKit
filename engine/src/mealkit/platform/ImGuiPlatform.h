#pragma once

#include "mealkit/core/Types.h"

// 디버그 UI(Dear ImGui)의 창·입력 연결. MEALKIT_DEBUG_UI 빌드에만 있다 (Windows: imgui_impl_win32).
// 그리기 쪽은 RHI(mealkit/rhi/Rhi.h의 imgui*)가 맡는다.
namespace mealkit {

class Window;

// ImGui 컨텍스트를 만든 뒤 부른다. 창 메시지를 ImGui에 넘기는 훅을 건다
bool imguiPlatformInit(Window& window);
void imguiPlatformShutdown(Window& window);
void imguiPlatformNewFrame();
f32 imguiDpiScale(Window& window); // 창이 있는 모니터의 배율 (1.0 = 96 DPI)

} // namespace mealkit
