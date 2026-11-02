#pragma once

#include "mealkit/core/Reflect.h"
#include "sim/Tables.h"

// Scene object kinds declared by the game (editor [Add] menu). The World reads them at match start (sim/World.cpp loadScene).
// Values that differ per map go into these objects instead of code constants (docs/CODING_MODEL.md "Where to put numbers").
namespace coop {

// Match area (one per scene). Width/depth = where chasers may go, bottom = falling below it sends a hero back to a spawn point
MEALKIT_OBJECT(MatchArea) {
    static constexpr const char* kObjectLabel = "경기 영역";
    static constexpr mealkit::u32 kObjectFlags = mealkit::reflect::kObjectArea;
};

// The goal (one per scene): the round is won when every connected hero stands inside it, none of them down
MEALKIT_OBJECT(GoalArea) {
    static constexpr const char* kObjectLabel = "목표 지점";
    static constexpr mealkit::u32 kObjectFlags = mealkit::reflect::kObjectArea;
};

// Where chasers appear at the start of a round (and come back after falling off)
MEALKIT_OBJECT(ChaserSpawn) {
    static constexpr const char* kObjectLabel = "추격자 생성 지점";
    MEALKIT_FIELD(mealkit::u32, count, 1, "추격자 수");
};

} // namespace coop
