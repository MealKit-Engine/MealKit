#pragma once

#include "sim/World.h"

#include "mealkit/core/Reflect.h"
#include "mealkit/scene/Scene.h"

#include <array>

// View tuning: assets/view.tuning (edited in the "View" tab of the editor [Tuning] panel). Holds display-only values (camera, colors).
// Not in the content hash, so it changes immediately even during multiplayer play. The view side reads it with ctx.viewTuning<ViewTuning>().
// Can't be read from sim/ (SimCheck blocks view/ headers). Game rule values are in sim/Tuning.h.
namespace sandbox {

using mealkit::Color;

// Camera before the match starts: slowly circles the scene center
inline mealkit::scene::VirtualCameraFields defaultMenuCamera() {
    mealkit::scene::VirtualCameraFields f;
    f.body = mealkit::scene::CameraBody::Orbit;
    f.distance = 20.0f;
    f.yaw = 45.0f;
    f.pitch = 30.0f;
    f.orbitSpeed = 4.0f;
    return f;
}

// 2D side view: orthographic, looking along +z at the XY plane from a fixed point (the camrig module has a follow camera)
MEALKIT_STRUCT(CameraTuning) {
    MEALKIT_FIELD(f32, halfHeight, 7.0f, "화면 높이의 절반 (m, 직교)");
    MEALKIT_FIELD(f32, pixelsPerUnit, 0.0f, "픽셀 아트 한 칸이 1m에서 차지하는 픽셀 수 (0: 픽셀 스냅 끔). 켜면 확대가 정수 배가 되고 카메라가 화면 픽셀에 맞춰 움직인다");
    MEALKIT_FIELD(mealkit::Vec3, center, (mealkit::Vec3{0.0f, 4.0f, 0.0f}), "화면 가운데 (m, 평면 위)");
    MEALKIT_FIELD(f32, nearPlane, -100.0f, "가까운 자르기 면 (m, 직교라 음수 가능)");
    MEALKIT_FIELD(f32, farPlane, 200.0f, "먼 자르기 면 (m)");
    MEALKIT_FIELD(mealkit::scene::VirtualCameraFields, menu, defaultMenuCamera(), "매치 시작 전 카메라");
};

using PlayerColors = std::array<Color, mealkit::session::kMaxPlayers>;

MEALKIT_STRUCT(ViewTuning) {
    MEALKIT_FIELD(CameraTuning, camera, {}, "카메라");
    MEALKIT_FIELD(PlayerColors, playerColors,
                  (PlayerColors{Color::rgb(66, 135, 245), Color::rgb(235, 64, 52), Color::rgb(52, 199, 89),
                                Color::rgb(250, 204, 21), Color::rgb(168, 85, 247), Color::rgb(236, 72, 153),
                                Color::rgb(20, 184, 166), Color::rgb(249, 115, 22)}),
                  "플레이어 색");
};

} // namespace sandbox
