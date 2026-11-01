#pragma once

#include "mealkit/core/Math.h"
#include "mealkit/core/Reflect.h"
#include "mealkit/sim/Movement.h"
#include "mealkit/sim/NavGrid.h"

// Game tuning values: assets/game.tuning (edited in the editor [Tuning] panel). The World copies it at match start and reads it as world.tuning.
// In multiplayer it's fixed at match start and goes into the content hash (peers using different values are rejected).
// In editor solo play, changing values applies immediately (World::setTuning).
// Values that differ per map (areas, counts, heights) go in scene objects (sim/Objects.h), values with many rows of the same shape (box kinds) in tables (sim/Tables.h),
// and display-only values in view/ViewTuning.h (docs/CODING_MODEL.md "Where to put numbers").
namespace sandbox {

MEALKIT_STRUCT(Tuning) {
    // Match
    MEALKIT_FIELD(mealkit::Ticks, countdown, mealkit::Ticks::seconds(3.0f), "시작 카운트다운 (초, 0이면 없음)");
    MEALKIT_FIELD(mealkit::Ticks, roundLength, mealkit::Ticks::seconds(120.0f), "라운드 시간 (초, 0이면 끝나지 않음)");
    MEALKIT_FIELD(mealkit::Ticks, roundEndDelay, mealkit::Ticks::seconds(3.0f), "라운드 결과를 보여 주는 시간 (초)");
    MEALKIT_FIELD(mealkit::u32, rounds, 1, "라운드 수 (과반을 이기면 매치 끝)");
    MEALKIT_FIELD(mealkit::u32, scoreLimit, 0, "이 점수에 닿으면 라운드 승리 (0이면 없음)");
    MEALKIT_FIELD(mealkit::u32, lives, 0, "목숨 (0이면 무한, 있으면 모두 잃은 쪽이 탈락)");
    MEALKIT_FIELD(mealkit::u32, teams, 0, "팀 수 (0·1이면 개인전, 슬롯 번호 % 팀 수)");
    MEALKIT_FIELD(mealkit::u32, minPlayers, 1, "이만큼 모일 때까지 대기");
    // Movement (engine movement modes, mealkit/sim/Movement.h: the mode is per body kind in tables/bodies.table)
    MEALKIT_FIELD(mealkit::sim::MoveTuning, move, {}, "이동");
    MEALKIT_FIELD(mealkit::Vec3, gravity, (mealkit::Vec3{0.0f, -20.0f, 0.0f}), "중력 (m/s²)");
    // Pushing
    MEALKIT_FIELD(mealkit::Ticks, pushCooldown, mealkit::Ticks::seconds(0.5f), "밀치기 재사용 대기 (초)");
    MEALKIT_FIELD(mealkit::f32, pushRadius, 7.0f, "밀치기 범위 (m)");
    MEALKIT_FIELD(mealkit::f32, pushSpeed, 18.0f, "밀치기 수평 속도 (m/s)");
    MEALKIT_FIELD(mealkit::f32, pushLift, 7.0f, "밀치기 위로 속도 (m/s)");
    MEALKIT_FIELD(mealkit::f32, pushSpin, 8.0f, "밀치기 회전 (rad/s)");
    // Respawn/AI
    MEALKIT_FIELD(mealkit::Ticks, respawn, mealkit::Ticks::seconds(1.5f), "리스폰 대기 (초)");
    MEALKIT_FIELD(mealkit::u32, aiPawns, 2, "AI 캐릭터 수");
    MEALKIT_FIELD(mealkit::Ticks, aiThink, mealkit::Ticks::seconds(1.3333f), "AI 판단 간격 (초)");
    MEALKIT_FIELD(mealkit::f32, aiSpeedScale, 0.7f, "AI 이동 속도 비율");
    MEALKIT_FIELD(mealkit::f32, aiTargetMin, 4.0f, "AI 목표 지점 최소 거리 (m)");
    MEALKIT_FIELD(mealkit::f32, aiTargetMax, 14.0f, "AI 목표 지점 최대 거리 (m)");
    MEALKIT_FIELD(mealkit::u32, aiJumpOneIn, 4, "AI가 목표를 바꿀 때 점프할 확률 (1/N)");
    MEALKIT_FIELD(mealkit::u32, aiPushOneIn, 3, "AI가 목표를 바꿀 때 밀칠 확률 (1/N)");
    MEALKIT_FIELD(mealkit::sim::NavBakeSettings, nav, {}, "AI 길찾기 그리드");
    MEALKIT_FIELD(mealkit::u32, aiPathsPerTick, 2, "틱당 AI 길찾기 수 (컨트롤러 순서)");
    MEALKIT_FIELD(mealkit::u32, aiPathExpand, 4096, "길찾기 한 번에 살펴보는 최대 칸 수");
    MEALKIT_FIELD(bool, navDebug, false, "길찾기 그리드와 AI 경로 그리기 (디버그)");
    // Character bodies
    MEALKIT_FIELD(mealkit::f32, pawnHalfHeight, 0.9f, "캐릭터 반 높이 (m)");
    MEALKIT_FIELD(mealkit::f32, pawnRadius, 0.45f, "캐릭터 반지름 (m)");
    MEALKIT_FIELD(mealkit::f32, pawnMass, 80.0f, "캐릭터 질량 (kg)");
    MEALKIT_FIELD(mealkit::f32, spawnSpacing, 1.2f, "스폰 지점보다 많을 때 옆으로 비켜 서는 거리 (m)");
    // Scene
    MEALKIT_FIELD(mealkit::f32, solidFriction, 0.8f, "씬 충돌 물체 마찰");
    MEALKIT_FIELD(mealkit::f32, padBelow, 0.3f, "점프대 윗면 아래로 감지 (m)");
    MEALKIT_FIELD(mealkit::f32, padAbove, 0.4f, "점프대 윗면 위로 감지 (m)");
    // View notifications (material for sound/effects. Not part of the state)
    MEALKIT_FIELD(mealkit::f32, impactSpeed, 3.0f, "충돌 이벤트를 내는 최소 속도 (m/s)");
    MEALKIT_FIELD(mealkit::u32, impactsPerTick, 8, "틱당 충돌 이벤트 수 (센 것부터)");
};

} // namespace sandbox
