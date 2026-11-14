#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/core/Types.h"

#include <string>

// Progress data (mealkit/game/Save.h): saves/progress.save in the user folder. Only records that persist outside matches; the simulation doesn't read it.
// Adding or removing fields still reads old files (missing fields get defaults). When a meaning changes, bump kSaveVersion.
namespace sandbox {

inline constexpr mealkit::u32 kSaveVersion = 1;

MEALKIT_STRUCT(SaveData) {
    MEALKIT_FIELD(mealkit::u32, matchesStarted, 0, "시작한 매치 수");
};

} // namespace sandbox
