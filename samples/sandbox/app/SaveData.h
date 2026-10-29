#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/core/Types.h"

#include <string>

// Progress data (mealkit/game/Save.h): saves/progress.save in the user folder. Only records that persist outside matches; the simulation doesn't read it.
// Adding or removing fields still reads old files (missing fields get defaults). When a meaning changes, bump kSaveVersion and move it in migrate.
namespace sandbox {

inline constexpr mealkit::u32 kSaveVersion = 1;

MEALKIT_STRUCT(SaveData) {
    MEALKIT_FIELD(mealkit::u32, matchesStarted, 0, "시작한 매치 수");
    MEALKIT_FIELD(mealkit::u32, falls, 0, "내가 떨어진 횟수 (모든 매치)");
    MEALKIT_FIELD(mealkit::u32, pushes, 0, "내 밀치기 수 (모든 매치)");
    MEALKIT_FIELD(mealkit::f32, playSeconds, 0.0f, "플레이 시간 (초)");
    MEALKIT_FIELD(mealkit::u32, body, 0, "고른 몸 종류 (bodies.table 행 번호, 다음 매치의 로드아웃)");
    MEALKIT_FIELD(std::string, playerName, {}, "로비에 보이는 내 이름 (비어 있으면 계정·사용자 이름)");
};

} // namespace sandbox
