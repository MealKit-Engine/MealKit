#include "view/Camera.h"

#include <algorithm>
#include <cmath>

using namespace mealkit;

namespace sandbox {
namespace {

constexpr f32 kPi = 3.14159265f; // 단위 변환 (도 → 라디안)

} // namespace

void OrbitCamera::control(Window& window, const input::Actions& actions, const CameraTuning& t) {
    if (!placed) {
        yaw = t.startYaw;
        pitch = t.startPitch;
        distance = t.startDistance;
        placed = true;
    }
    if (actions.pressed("orbit")) orbiting = true;
    if (!actions.down("orbit") || !window.focused()) orbiting = false;
    window.setCursorCaptured(orbiting);
    Vec2 look = actions.axis2("turn"); // 게임패드 스틱: 누르지 않고 바로 (rate = 초당 픽셀 환산)
    if (orbiting) look += actions.axis2("look"); // 마우스는 픽셀, 위가 +
    yaw += look.x * t.orbitSensitivity;
    pitch -= look.y * t.orbitSensitivity;
    pitch = std::clamp(pitch, t.minPitch, t.maxPitch); // 튜닝을 바꾸면 범위 안으로 돌아온다
    distance -= actions.axis("zoom") * t.zoomStep;
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
    frame.cameraForward = normalize(focus - from); // 소리 리스너 방향
}

} // namespace sandbox
