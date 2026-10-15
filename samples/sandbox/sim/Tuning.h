#pragma once

#include "mealkit/core/Math.h"
#include "mealkit/core/Reflect.h"

// 게임 튜닝 값: assets/game.tuning (에디터 [튜닝] 패널에서 고친다). World가 매치를 시작할 때 복사해 world.tuning으로 읽는다.
// 멀티플레이에서는 매치 시작 때 고정되고 콘텐츠 해시에 들어간다(다른 값을 쓰는 피어는 접속 거부).
// 에디터 혼자 플레이에서는 값을 바꾸면 바로 적용된다 (World::setTuning).
namespace sandbox {

MEALKIT_STRUCT(Tuning) {
    MEALKIT_FIELD(mealkit::f32, playerSpeed, 7.0f, "이동 속도 (m/s)");
    MEALKIT_FIELD(mealkit::f32, groundAccel, 12.0f, "지상 가속 (1/s)");
    MEALKIT_FIELD(mealkit::f32, airAccel, 3.0f, "공중 가속 (1/s)");
    MEALKIT_FIELD(mealkit::f32, jumpSpeed, 8.0f, "점프 속도 (m/s)");
    MEALKIT_FIELD(mealkit::Vec3, gravity, (mealkit::Vec3{0.0f, -20.0f, 0.0f}), "중력 (m/s²)");
    MEALKIT_FIELD(mealkit::u32, pushCooldownTicks, 15, "밀치기 재사용 대기 (틱)");
    MEALKIT_FIELD(mealkit::f32, pushRadius, 7.0f, "밀치기 범위 (m)");
    MEALKIT_FIELD(mealkit::f32, pushSpeed, 18.0f, "밀치기 수평 속도 (m/s)");
    MEALKIT_FIELD(mealkit::f32, pushLift, 7.0f, "밀치기 위로 속도 (m/s)");
    MEALKIT_FIELD(mealkit::f32, pushSpin, 8.0f, "밀치기 회전 (rad/s)");
    MEALKIT_FIELD(mealkit::f32, propDensity, 200.0f, "상자 밀도 (kg/m³)");
    MEALKIT_FIELD(mealkit::u32, respawnTicks, 45, "리스폰 대기 (틱)");
    MEALKIT_FIELD(mealkit::u32, aiPawns, 2, "AI 캐릭터 수");
    MEALKIT_FIELD(mealkit::u32, aiThinkTicks, 40, "AI 판단 간격 (틱)");
    MEALKIT_FIELD(mealkit::f32, aiSpeedScale, 0.7f, "AI 이동 속도 비율");
};

} // namespace sandbox
