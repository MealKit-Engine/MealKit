#pragma once

#include "mealkit/core/Reflect.h"

// 게임이 선언한 씬 오브젝트 종류. 에디터 [추가] 메뉴와 인스펙터에 나오고, 필드 값은 씬 파일에 저장된다.
// World는 매치를 시작할 때 scene::readObject<T>(entity)로 읽는다 (sim/World.cpp loadScene).
// 맵마다 다른 값(범위, 개수, 높이)은 코드 상수 대신 여기 물체의 위치·크기·필드로 둔다 (docs/CODING_MODEL.md "숫자는 어디에 두나").
namespace sandbox {

// 밟으면 위로 튕겨 올리는 발판. 씬에서 놓은 상자의 윗면이 밟는 곳이다 (회전은 무시).
MEALKIT_OBJECT(JumpPad) {
    static constexpr const char* kObjectLabel = "점프대";
    MEALKIT_FIELD(mealkit::f32, power, 14.0f, "점프 힘 (m/s)");
    MEALKIT_FIELD(mealkit::Ticks, cooldown, mealkit::Ticks::seconds(0.6667f), "재사용 대기 (초)");
};

// 경기 영역 (씬에 하나). 상자의 크기가 곧 규칙이다 (회전은 무시):
//   가로·세로 = AI가 돌아다니는 범위, 떨어진 상자를 다시 떨어뜨리는 범위
//   바닥 = 이보다 아래로 떨어지면 낙사 / 위 = 떨어진 상자를 다시 떨어뜨리는 높이
MEALKIT_OBJECT(MatchArea) {
    static constexpr const char* kObjectLabel = "경기 영역";
    static constexpr mealkit::u32 kObjectFlags = mealkit::reflect::kObjectArea;
};

// 매치를 시작할 때 이 영역 안에 상자를 흩어 놓는다 (여러 개면 씬 순서대로)
MEALKIT_OBJECT(PropSpawnArea) {
    static constexpr const char* kObjectLabel = "상자 생성 영역";
    static constexpr mealkit::u32 kObjectFlags = mealkit::reflect::kObjectArea;
    MEALKIT_FIELD(mealkit::u32, count, 2000, "상자 수");
};

} // namespace sandbox
