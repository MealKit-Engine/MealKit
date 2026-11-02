#pragma once

#include "mealkit/core/Math.h"
#include "mealkit/core/Reflect.h"
#include "mealkit/sim/Movement.h"
#include "mealkit/sim/NavGrid.h"

// Game tuning values: assets/game.tuning (editor [Tuning] panel). The World copies it at match start and reads it as world.tuning.
// In multiplayer it's fixed at match start and goes into the content hash. Per-map values go in scene objects (sim/Objects.h),
// hero body kinds in tables/bodies.table, display-only values in view/ViewTuning.h.
namespace coop {

MEALKIT_STRUCT(Tuning) {
    // Match (engine mealkit/sim/MatchFlow.h with coop rules: everyone wins or loses together)
    MEALKIT_FIELD(mealkit::Ticks, countdown, mealkit::Ticks::seconds(3.0f), "시작 카운트다운 (초, 0이면 없음)");
    MEALKIT_FIELD(mealkit::Ticks, roundLength, mealkit::Ticks::seconds(180.0f), "제한 시간 (초, 0이면 없음)");
    MEALKIT_FIELD(mealkit::Ticks, roundEndDelay, mealkit::Ticks::seconds(4.0f), "결과를 보여 주는 시간 (초)");
    MEALKIT_FIELD(mealkit::u32, rounds, 1, "라운드 수");
    MEALKIT_FIELD(mealkit::u32, minPlayers, 1, "이만큼 모일 때까지 대기");
    MEALKIT_FIELD(mealkit::Ticks, goalHold, mealkit::Ticks::seconds(1.0f), "모두 목표 안에 이만큼 있으면 성공 (초)");
    // Heroes (movement mode per body kind in tables/bodies.table)
    MEALKIT_FIELD(mealkit::sim::MoveTuning, move, {}, "영웅 이동");
    MEALKIT_FIELD(mealkit::Vec3, gravity, (mealkit::Vec3{0.0f, -20.0f, 0.0f}), "중력 (m/s²)");
    MEALKIT_FIELD(mealkit::f32, heroHalfHeight, 0.9f, "영웅 반 높이 (m)");
    MEALKIT_FIELD(mealkit::f32, heroRadius, 0.45f, "영웅 반지름 (m)");
    MEALKIT_FIELD(mealkit::f32, heroMass, 80.0f, "영웅 질량 (kg)");
    MEALKIT_FIELD(mealkit::f32, spawnSpacing, 1.2f, "스폰 지점보다 많을 때 옆으로 비켜 서는 거리 (m)");
    MEALKIT_FIELD(mealkit::Ticks, respawn, mealkit::Ticks::seconds(2.0f), "떨어진 뒤 다시 나타나는 시간 (초)");
    // Shove (push button): stuns chasers nearby and throws them back
    MEALKIT_FIELD(mealkit::Ticks, shoveCooldown, mealkit::Ticks::seconds(1.5f), "밀치기 재사용 대기 (초)");
    MEALKIT_FIELD(mealkit::f32, shoveRadius, 3.5f, "밀치기 범위 (m)");
    MEALKIT_FIELD(mealkit::f32, shoveSpeed, 12.0f, "밀치기 수평 속도 (m/s)");
    MEALKIT_FIELD(mealkit::f32, shoveLift, 5.0f, "밀치기 위로 속도 (m/s)");
    MEALKIT_FIELD(mealkit::Ticks, shoveStun, mealkit::Ticks::seconds(2.5f), "밀친 추격자가 멈춰 있는 시간 (초)");
    // Caught and revived
    MEALKIT_FIELD(mealkit::f32, catchRadius, 1.1f, "추격자가 잡는 거리 (m)");
    MEALKIT_FIELD(mealkit::f32, catchKnock, 6.0f, "잡힐 때 밀려나는 속도 (m/s)");
    MEALKIT_FIELD(mealkit::Ticks, catchRest, mealkit::Ticks::seconds(4.0f), "잡은 뒤 추격자가 쉬는 시간 (초)");
    MEALKIT_FIELD(mealkit::f32, reviveRadius, 2.5f, "부활시키는 거리 (m)");
    MEALKIT_FIELD(mealkit::Ticks, reviveTime, mealkit::Ticks::seconds(1.5f), "부활에 걸리는 시간 (초)");
    // Chasers (AI on the nav grid)
    MEALKIT_FIELD(mealkit::sim::MoveTuning, chaserMove, {}, "추격자 이동");
    MEALKIT_FIELD(mealkit::f32, chaserSpeedScale, 0.72f, "추격자 이동 속도 비율");
    MEALKIT_FIELD(mealkit::f32, chaserHalfHeight, 0.8f, "추격자 반 높이 (m)");
    MEALKIT_FIELD(mealkit::f32, chaserRadius, 0.5f, "추격자 반지름 (m)");
    MEALKIT_FIELD(mealkit::f32, chaserMass, 120.0f, "추격자 질량 (kg)");
    MEALKIT_FIELD(mealkit::f32, sightRadius, 22.0f, "추격자가 영웅을 알아채는 거리 (m)");
    MEALKIT_FIELD(mealkit::Ticks, aiThink, mealkit::Ticks::seconds(0.75f), "추격자 판단 간격 (초)");
    MEALKIT_FIELD(mealkit::f32, wanderRadius, 8.0f, "대상이 없을 때 돌아다니는 거리 (m)");
    MEALKIT_FIELD(mealkit::sim::NavBakeSettings, nav, {}, "길찾기 그리드");
    MEALKIT_FIELD(mealkit::u32, aiPathsPerTick, 2, "틱당 길찾기 수 (컨트롤러 순서)");
    MEALKIT_FIELD(mealkit::u32, aiPathExpand, 4096, "길찾기 한 번에 살펴보는 최대 칸 수");
    MEALKIT_FIELD(bool, navDebug, false, "길찾기 경로 그리기 (디버그)");
    // Scene and view notifications
    MEALKIT_FIELD(mealkit::f32, solidFriction, 0.8f, "씬 충돌 물체 마찰");
    MEALKIT_FIELD(mealkit::f32, impactSpeed, 4.0f, "충돌 이벤트를 내는 최소 속도 (m/s)");
    MEALKIT_FIELD(mealkit::u32, impactsPerTick, 6, "틱당 충돌 이벤트 수 (센 것부터)");
};

} // namespace coop
