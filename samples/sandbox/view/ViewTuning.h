#pragma once

#include "sim/World.h"

#include "mealkit/core/Reflect.h"

#include <array>

// View tuning: assets/view.tuning (edited in the "View" tab of the editor [Tuning] panel). Holds display-only values (camera, colors, effects).
// Not in the content hash, so it changes immediately even during multiplayer play. The view side (view/) reads it with ctx.viewTuning<ViewTuning>().
// Can't be read from sim/ (SimCheck blocks view/ headers). Game rule values are in sim/Tuning.h.
namespace sandbox {

using mealkit::Color;

MEALKIT_STRUCT(CameraTuning) {
    MEALKIT_FIELD(f32, fovDegrees, 60.0f, "세로 시야각 (도)");
    MEALKIT_FIELD(f32, orbitSensitivity, 0.005f, "회전 감도 (rad/픽셀)");
    MEALKIT_FIELD(f32, zoomStep, 2.0f, "휠 한 칸 줌 (m)");
    MEALKIT_FIELD(f32, startDistance, 18.0f, "처음 거리 (m)");
    MEALKIT_FIELD(f32, minDistance, 4.0f, "가장 가까운 거리 (m)");
    MEALKIT_FIELD(f32, maxDistance, 80.0f, "가장 먼 거리 (m)");
    MEALKIT_FIELD(f32, startYaw, 0.8f, "처음 방향 (rad)");
    MEALKIT_FIELD(f32, startPitch, 0.55f, "처음 높이 각 (rad)");
    MEALKIT_FIELD(f32, minPitch, -0.1f, "가장 낮은 높이 각 (rad)");
    MEALKIT_FIELD(f32, maxPitch, 1.45f, "가장 높은 높이 각 (rad)");
    MEALKIT_FIELD(f32, focusHeight, 0.8f, "캐릭터 위 바라보는 높이 (m)");
    MEALKIT_FIELD(f32, nearPlane, 0.1f, "가까운 자르기 면 (m)");
    MEALKIT_FIELD(f32, farPlane, 1000.0f, "먼 자르기 면 (m)");
};

// Character appearance (a combination of boxes. Switch to a model when model assets exist)
MEALKIT_STRUCT(PawnLook) {
    MEALKIT_FIELD(Vec3, bodySize, (Vec3{0.8f, 1.8f, 0.8f}), "몸 크기 (m)");
    MEALKIT_FIELD(Vec3, visorOffset, (Vec3{0.0f, 0.45f, 0.4f}), "얼굴 위치 (몸 기준)");
    MEALKIT_FIELD(Vec3, visorSize, (Vec3{0.5f, 0.18f, 0.12f}), "얼굴 크기 (m)");
    MEALKIT_FIELD(Color, visorColor, (Color::rgb(30, 30, 36)), "얼굴 색");
    MEALKIT_FIELD(Color, aiColor, (Color::rgb(150, 150, 160)), "AI 캐릭터 색");
    MEALKIT_FIELD(f32, turnMinSpeed, 0.5f, "바라보는 방향을 바꾸는 최소 속도 (m/s)");
    MEALKIT_FIELD(f32, markerHeight, 1.6f, "내 캐릭터 표시 높이 (m)");
    MEALKIT_FIELD(f32, markerSize, 0.3f, "내 캐릭터 표시 크기 (m)");
    MEALKIT_FIELD(f32, markerSpin, 3.0f, "내 캐릭터 표시 회전 (rad/s)");
    MEALKIT_FIELD(Color, markerColor, (Color::rgb(255, 230, 80)), "내 캐릭터 표시 색");
    MEALKIT_FIELD(f32, hitFlash, 0.8f, "밀쳐졌을 때 번쩍임 세기 (0..1)");
    MEALKIT_FIELD(f32, hitFlashSeconds, 0.2f, "밀쳐졌을 때 번쩍임이 사라지는 시간 (초)");
    // Skinned model (if empty, box body + face). Feet are at the origin, drawn footOffset below the body center
    MEALKIT_FIELD(std::string, model, "models/characters/rig_figure.glb", "캐릭터 모델 (스킨, 비면 상자)");
    MEALKIT_FIELD(f32, footOffset, 0.9f, "몸 중심에서 발까지 (m)");
    MEALKIT_FIELD(std::string, idleClip, "idle", "서 있을 때 클립");
    MEALKIT_FIELD(std::string, runClip, "run", "움직일 때 클립");
    MEALKIT_FIELD(f32, runSpeed, 1.0f, "이 속도(m/s)보다 빠르면 달리기 클립");
    MEALKIT_FIELD(f32, blendSeconds, 0.15f, "클립을 바꿀 때 섞는 시간 (초)");
};

MEALKIT_STRUCT(ShockwaveLook) {
    MEALKIT_FIELD(f32, seconds, 0.35f, "퍼지는 시간 (초)");
    MEALKIT_FIELD(mealkit::u32, segments, 16, "고리 조각 수");
    MEALKIT_FIELD(f32, startThickness, 0.3f, "처음 두께 (m)");
    MEALKIT_FIELD(f32, endThickness, 0.05f, "끝 두께 (m)");
    MEALKIT_FIELD(f32, groundOffset, 0.05f, "바닥 위 높이 (m)");
    MEALKIT_FIELD(f32, spawnRadius, 1.2f, "스폰 고리 크기 (m)");
    MEALKIT_FIELD(f32, bounceRadius, 2.0f, "점프대 고리 크기 (m)");
    MEALKIT_FIELD(Color, color, (Color::rgb(255, 240, 200)), "색");
};

// Moves only my character's appearance ahead (hides input delay, the simulation is unchanged): drawn ahead by my inputs sent but not yet executed
MEALKIT_STRUCT(PredictionLook) {
    MEALKIT_FIELD(bool, enabled, true, "켜기");
    MEALKIT_FIELD(f32, maxOffset, 1.5f, "확정 위치에서 가장 멀리 (m, 충돌로 어긋나도 크게 튀지 않게)");
    MEALKIT_FIELD(f32, follow, 14.0f, "따라가는 빠르기 (1/s, 클수록 바로)");
};

using PlayerColors = std::array<Color, mealkit::net::kMaxPlayers>;
using PropColors = std::array<Color, kPropColorCount>;

MEALKIT_STRUCT(ViewTuning) {
    MEALKIT_FIELD(CameraTuning, camera, {}, "카메라");
    MEALKIT_FIELD(PawnLook, pawn, {}, "캐릭터 겉모습");
    MEALKIT_FIELD(ShockwaveLook, shockwave, {}, "충격파 효과");
    MEALKIT_FIELD(PredictionLook, prediction, {}, "내 캐릭터 미리 움직이기");
    MEALKIT_FIELD(PlayerColors, playerColors,
                  (PlayerColors{Color::rgb(66, 135, 245), Color::rgb(235, 64, 52), Color::rgb(52, 199, 89),
                                Color::rgb(250, 204, 21), Color::rgb(168, 85, 247), Color::rgb(236, 72, 153),
                                Color::rgb(20, 184, 166), Color::rgb(249, 115, 22)}),
                  "플레이어 색");
    MEALKIT_FIELD(f32, nameTagHeight, 2.1f, "이름표 높이 (캐릭터 발 위, m)");
    MEALKIT_FIELD(f32, menuOrbitSpeed, 0.08f, "메뉴 뒤 카메라가 도는 속도 (rad/s)");
    MEALKIT_FIELD(f32, resultsSeconds, 20.0f, "결과 화면 뒤 방장이 자동으로 로비로 돌아가기까지 (초)");
    MEALKIT_FIELD(f32, chatShowSeconds, 8.0f, "매치 중 새 채팅 줄을 보여 주는 시간 (초)");
    MEALKIT_FIELD(mealkit::u32, badPingMs, 150, "핑을 나쁨(빨강)으로 보이는 왕복 시간 (ms)");
    MEALKIT_FIELD(PropColors, propColors,
                  (PropColors{Color::rgb(232, 98, 82), Color::rgb(245, 176, 65), Color::rgb(92, 172, 238),
                              Color::rgb(120, 200, 120), Color::rgb(190, 120, 220), Color::rgb(236, 236, 230)}),
                  "상자 색");
};

} // namespace sandbox
