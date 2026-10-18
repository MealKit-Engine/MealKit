#pragma once

#include "mealkit/core/Reflect.h"

// 게임이 선언한 씬 오브젝트 종류. 에디터 [추가] 메뉴와 인스펙터에 나오고, 필드 값은 씬 파일에 저장된다.
// World는 매치를 시작할 때 scene::readObject<T>(entity)로 읽는다 (sim/World.cpp loadScene).
namespace sandbox {

// 밟으면 위로 튕겨 올리는 발판. 씬에서 놓은 상자의 윗면이 밟는 곳이다 (회전은 무시).
MEALKIT_OBJECT(JumpPad) {
    static constexpr const char* kObjectLabel = "점프대";
    MEALKIT_FIELD(mealkit::f32, power, 14.0f, "점프 힘 (m/s)");
    MEALKIT_FIELD(mealkit::Ticks, cooldown, mealkit::Ticks::seconds(0.6667f), "재사용 대기 (초)");
};

} // namespace sandbox
