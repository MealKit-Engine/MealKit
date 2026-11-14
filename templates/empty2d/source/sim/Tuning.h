#pragma once

#include "mealkit/core/Math.h"
#include "mealkit/core/Reflect.h"
#include "mealkit/sim/Movement.h"

// Game tuning values: assets/game.tuning (edited in the editor [Tuning] panel). The World copies it at match start and reads it as world.tuning.
// In multiplayer it's fixed at match start and goes into the content hash (peers using different values are rejected).
// Values that differ per map go in scene objects (sim/Objects.h), display-only values in view/ViewTuning.h (engine docs/CODING_MODEL.md "Where to put numbers").
namespace sandbox {

MEALKIT_STRUCT(Tuning) {
    // Characters (engine movement modes, mealkit/sim/Movement.h)
    MEALKIT_FIELD(mealkit::sim::MoveTuning, move, {}, "이동");
    MEALKIT_FIELD(mealkit::Vec3, gravity, (mealkit::Vec3{0.0f, -20.0f, 0.0f}), "중력 (m/s²)");
    MEALKIT_FIELD(mealkit::f32, pawnHalfHeight, 0.9f, "캐릭터 반 높이 (m)");
    MEALKIT_FIELD(mealkit::f32, pawnRadius, 0.45f, "캐릭터 반지름 (m)");
    MEALKIT_FIELD(mealkit::f32, pawnMass, 80.0f, "캐릭터 질량 (kg)");
    MEALKIT_FIELD(mealkit::f32, spawnSpacing, 1.2f, "스폰 지점보다 많을 때 옆으로 비켜 서는 거리 (m)");
    MEALKIT_FIELD(mealkit::f32, fallHeight, -20.0f, "이 높이 아래로 떨어지면 리스폰 (m)");
    MEALKIT_FIELD(mealkit::Ticks, respawn, mealkit::Ticks::seconds(1.5f), "리스폰 대기 (초)");
    // Scene
    MEALKIT_FIELD(mealkit::f32, solidFriction, 0.8f, "씬 충돌 물체 마찰");
};

} // namespace sandbox
