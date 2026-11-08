#pragma once

#include "mealkit/core/Types.h"

// Window/input glue for the debug UI (Dear ImGui). Only in MEALKIT_DEBUG_UI builds (Windows: imgui_impl_win32).
// The drawing side is handled by the RHI (imgui* in mealkit/rhi/Rhi.h).
namespace mealkit {

class Window;

// Call after creating the ImGui context. Installs a hook that passes window messages to ImGui
bool imguiPlatformInit(Window& window);
void imguiPlatformShutdown(Window& window);
void imguiPlatformNewFrame();
f32 imguiDpiScale(Window& window); // scale of the monitor the window is on (1.0 = 96 DPI)

} // namespace mealkit
