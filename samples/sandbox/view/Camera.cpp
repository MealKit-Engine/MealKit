#include "view/Camera.h"

#include <algorithm>
#include <cmath>

using namespace mealkit;

namespace sandbox {
namespace {

constexpr f32 kPi = 3.14159265f; // 단위 변환 (도 → 라디안)

} // namespace

void OrbitCamera::control(Window& window, bool mouseAllowed, const CameraTuning& t) {
    if (!placed) {
        yaw = t.startYaw;
        pitch = t.startPitch;
        distance = t.startDistance;
        placed = true;
    }
    Input& input = window.input();
    if (mouseAllowed && input.mousePressed(MouseButton::Right)) orbiting = true;
    if (!input.mouseDown(MouseButton::Right) || !window.focused()) orbiting = false;
    window.setCursorCaptured(orbiting);
    if (orbiting) {
        yaw += input.mouseDeltaX() * t.orbitSensitivity;
        pitch += input.mouseDeltaY() * t.orbitSensitivity;
    }
    pitch = std::clamp(pitch, t.minPitch, t.maxPitch); // 튜닝을 바꾸면 범위 안으로 돌아온다
    if (mouseAllowed) distance -= input.wheelDelta() * t.zoomStep;
    distance = std::clamp(distance, t.minDistance, t.maxDistance);
}

Vec3 OrbitCamera::forward() const { return {-std::sin(yaw), 0.0f, -std::cos(yaw)}; }

Vec3 OrbitCamera::eye(const Vec3& focus) const {
    return focus + Vec3{std::sin(yaw) * std::cos(pitch), std::sin(pitch), std::cos(yaw) * std::cos(pitch)} * distance;
}

void OrbitCamera::apply(const Vec3& focus, render::Viewport viewport, const CameraTuning& t,
                        render::FrameParams& frame) const {
    const Vec3 from = eye(focus);
    const f32 width = viewport.width > 0.0f ? viewport.width : 1.0f;
    const f32 height = viewport.height > 0.0f ? viewport.height : 1.0f;
    frame.viewport = viewport;
    const f32 fovY = t.fovDegrees * (kPi / 180.0f);
    frame.viewProj =
        lookAtLH(from, focus, Vec3{0, 1, 0}) * perspectiveReversedZ(fovY, width / height, t.nearPlane, t.farPlane);
    frame.cameraPos = from;
}

} // namespace sandbox
