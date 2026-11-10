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

// Third-person camera when the scene has no active virtual camera (scene virtual_camera objects override it)
inline mealkit::scene::VirtualCameraFields defaultFollowCamera() {
    mealkit::scene::VirtualCameraFields f;
    f.yaw = 45.0f;
    f.pitch = 30.0f;
    f.collisionRadius = 0.3f;
    return f;
}

// Camera behind the menu/lobby: slowly circles the scene center
inline mealkit::scene::VirtualCameraFields defaultMenuCamera() {
    mealkit::scene::VirtualCameraFields f;
    f.body = mealkit::scene::CameraBody::Orbit;
    f.distance = 20.0f;
    f.yaw = 45.0f;
    f.pitch = 30.0f;
    f.orbitSpeed = 4.0f;
    return f;
}

// 2D side view: orthographic, looking along +z at the XY plane, following my character
MEALKIT_STRUCT(CameraTuning) {
    MEALKIT_FIELD(f32, halfHeight, 7.0f, "화면 높이의 절반 (m, 직교)");
    MEALKIT_FIELD(f32, minHalfHeight, 3.0f, "가장 가까운 줌 (m)");
    MEALKIT_FIELD(f32, maxHalfHeight, 20.0f, "가장 먼 줌 (m)");
    MEALKIT_FIELD(f32, zoomStep, 1.0f, "휠 한 칸 줌 (m)");
    MEALKIT_FIELD(f32, followHeight, 2.0f, "캐릭터보다 위를 본다 (m)");
    MEALKIT_FIELD(f32, followSharpness, 6.0f, "따라가는 빠르기 (1/초)");
    MEALKIT_FIELD(f32, nearPlane, -100.0f, "가까운 자르기 면 (m, 직교라 음수 가능)");
    MEALKIT_FIELD(f32, farPlane, 200.0f, "먼 자르기 면 (m)");
    MEALKIT_FIELD(mealkit::scene::VirtualCameraFields, fallback, defaultFollowCamera(), "기본 카메라 (사용 안 함, 2D 카메라가 대신)");
    MEALKIT_FIELD(mealkit::scene::VirtualCameraFields, menu, defaultMenuCamera(), "메뉴 뒤 카메라");
};

using PlayerColors = std::array<Color, mealkit::session::kMaxPlayers>;

MEALKIT_STRUCT(ViewTuning) {
    MEALKIT_FIELD(CameraTuning, camera, {}, "카메라");
    MEALKIT_FIELD(Vec3, pawnSize, (Vec3{0.8f, 1.8f, 0.8f}), "캐릭터 크기 (m, 상자로 그린다)");
    MEALKIT_FIELD(PlayerColors, playerColors,
                  (PlayerColors{Color::rgb(66, 135, 245), Color::rgb(235, 64, 52), Color::rgb(52, 199, 89),
                                Color::rgb(250, 204, 21), Color::rgb(168, 85, 247), Color::rgb(236, 72, 153),
                                Color::rgb(20, 184, 166), Color::rgb(249, 115, 22)}),
                  "플레이어 색");
    MEALKIT_FIELD(f32, resultsSeconds, 20.0f, "결과 화면 뒤 방장이 자동으로 로비로 돌아가기까지 (초)");
    MEALKIT_FIELD(f32, chatShowSeconds, 8.0f, "매치 중 새 채팅 줄을 보여 주는 시간 (초)");
};

} // namespace sandbox
