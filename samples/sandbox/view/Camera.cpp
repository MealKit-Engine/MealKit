#include "view/Camera.h"

#include <algorithm>
#include <cmath>

using namespace mealkit;

namespace sandbox {
namespace {

constexpr f32 kFovY = 60.0f * 3.14159265f / 180.0f;

} // namespace

void OrbitCamera::control(Window& window, bool mouseAllowed) {
    Input& input = window.input();
    if (mouseAllowed && input.mousePressed(MouseButton::Right)) orbiting = true;
    if (!input.mouseDown(MouseButton::Right) || !window.focused()) orbiting = false;
    window.setCursorCaptured(orbiting);
    if (orbiting) {
        yaw += input.mouseDeltaX() * 0.005f;
        pitch = std::clamp(pitch + input.mouseDeltaY() * 0.005f, -0.1f, 1.45f);
    }
    if (mouseAllowed) distance = std::clamp(distance - input.wheelDelta() * 2.0f, 4.0f, 80.0f);
}

Vec3 OrbitCamera::forward() const { return {-std::sin(yaw), 0.0f, -std::cos(yaw)}; }

Vec3 OrbitCamera::eye(const Vec3& focus) const {
    return focus + Vec3{std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)} * distance;
}

void OrbitCamera::apply(const Vec3& focus, render::Viewport viewport, render::FrameParams& frame) const {
    const Vec3 from = eye(focus);
    const f32 width = viewport.width > 0.0f ? viewport.width : 1.0f;
    const f32 height = viewport.height > 0.0f ? viewport.height : 1.0f;
    frame.viewport = viewport;
    frame.viewProj = lookAtLH(from, focus, Vec3{0, 1, 0}) * perspectiveReversedZ(kFovY, width / height, 0.1f, 1000.0f);
    frame.cameraPos = from;
}

} // namespace sandbox
