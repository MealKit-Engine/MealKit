#pragma once

#include "mealkit/core/Math.h"
#include "mealkit/core/Reflect.h"

// 게임 튜닝 값: assets/game.tuning (에디터 [튜닝] 패널에서 고친다). World가 매치를 시작할 때 복사해 world.tuning으로 읽는다.
// 멀티플레이에서는 매치 시작 때 고정되고 콘텐츠 해시에 들어간다(다른 값을 쓰는 피어는 접속 거부).
// 에디터 혼자 플레이에서는 값을 바꾸면 바로 적용된다 (World::setTuning).
// 맵마다 다른 값(영역, 개수, 높이)은 씬 물체(sim/Objects.h), 같은 모양이 여러 행인 값(상자 종류)은 표(sim/Tables.h),
// 보이기만 하는 값은 view/ViewTuning.h (docs/CODING_MODEL.md "숫자는 어디에 두나").
namespace sandbox {

MEALKIT_STRUCT(Tuning) {
    // 매치
    MEALKIT_FIELD(mealkit::Ticks, countdown, mealkit::Ticks::seconds(3.0f), "시작 카운트다운 (초, 0이면 없음)");
    MEALKIT_FIELD(mealkit::Ticks, matchLength, mealkit::Ticks::seconds(120.0f), "경기 시간 (초, 0이면 끝나지 않음)");
    // 이동
    MEALKIT_FIELD(mealkit::f32, playerSpeed, 7.0f, "이동 속도 (m/s)");
    MEALKIT_FIELD(mealkit::f32, groundAccel, 12.0f, "지상 가속 (1/s)");
    MEALKIT_FIELD(mealkit::f32, airAccel, 3.0f, "공중 가속 (1/s)");
    MEALKIT_FIELD(mealkit::f32, jumpSpeed, 8.0f, "점프 속도 (m/s)");
    MEALKIT_FIELD(mealkit::Vec3, gravity, (mealkit::Vec3{0.0f, -20.0f, 0.0f}), "중력 (m/s²)");
    // 밀치기
    MEALKIT_FIELD(mealkit::Ticks, pushCooldown, mealkit::Ticks::seconds(0.5f), "밀치기 재사용 대기 (초)");
    MEALKIT_FIELD(mealkit::f32, pushRadius, 7.0f, "밀치기 범위 (m)");
    MEALKIT_FIELD(mealkit::f32, pushSpeed, 18.0f, "밀치기 수평 속도 (m/s)");
    MEALKIT_FIELD(mealkit::f32, pushLift, 7.0f, "밀치기 위로 속도 (m/s)");
    MEALKIT_FIELD(mealkit::f32, pushSpin, 8.0f, "밀치기 회전 (rad/s)");
    // 리스폰·AI
    MEALKIT_FIELD(mealkit::Ticks, respawn, mealkit::Ticks::seconds(1.5f), "리스폰 대기 (초)");
    MEALKIT_FIELD(mealkit::u32, aiPawns, 2, "AI 캐릭터 수");
    MEALKIT_FIELD(mealkit::Ticks, aiThink, mealkit::Ticks::seconds(1.3333f), "AI 판단 간격 (초)");
    MEALKIT_FIELD(mealkit::f32, aiSpeedScale, 0.7f, "AI 이동 속도 비율");
    MEALKIT_FIELD(mealkit::f32, aiTargetMin, 4.0f, "AI 목표 지점 최소 거리 (m)");
    MEALKIT_FIELD(mealkit::f32, aiTargetMax, 14.0f, "AI 목표 지점 최대 거리 (m)");
    MEALKIT_FIELD(mealkit::u32, aiJumpOneIn, 4, "AI가 목표를 바꿀 때 점프할 확률 (1/N)");
    MEALKIT_FIELD(mealkit::u32, aiPushOneIn, 3, "AI가 목표를 바꿀 때 밀칠 확률 (1/N)");
    // 캐릭터 몸
    MEALKIT_FIELD(mealkit::f32, pawnHalfHeight, 0.9f, "캐릭터 반 높이 (m)");
    MEALKIT_FIELD(mealkit::f32, pawnRadius, 0.45f, "캐릭터 반지름 (m)");
    MEALKIT_FIELD(mealkit::f32, pawnMass, 80.0f, "캐릭터 질량 (kg)");
    MEALKIT_FIELD(mealkit::f32, groundProbe, 0.12f, "발밑 땅 감지 여유 (m)");
    MEALKIT_FIELD(mealkit::f32, jumpMaxRise, 1.0f, "이보다 빨리 올라가는 중에는 점프하지 않음 (m/s)");
    MEALKIT_FIELD(mealkit::f32, spawnSpacing, 1.2f, "스폰 지점보다 많을 때 옆으로 비켜 서는 거리 (m)");
    // 씬
    MEALKIT_FIELD(mealkit::f32, solidFriction, 0.8f, "씬 충돌 물체 마찰");
    MEALKIT_FIELD(mealkit::f32, padBelow, 0.3f, "점프대 윗면 아래로 감지 (m)");
    MEALKIT_FIELD(mealkit::f32, padAbove, 0.4f, "점프대 윗면 위로 감지 (m)");
    // 화면 알림 (소리·효과의 재료. 상태에는 들어가지 않는다)
    MEALKIT_FIELD(mealkit::f32, impactSpeed, 3.0f, "충돌 이벤트를 내는 최소 속도 (m/s)");
    MEALKIT_FIELD(mealkit::u32, impactsPerTick, 8, "틱당 충돌 이벤트 수 (센 것부터)");
};

} // namespace sandbox
