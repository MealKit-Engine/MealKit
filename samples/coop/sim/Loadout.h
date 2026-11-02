#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/core/Types.h"

// Match start data (mealkit/game/Loadout.h): each player picks it before the match, and the host confirms it at start and sends it to everyone.
// Match start (Headless::startWorld) and the late join command (sim/Commands.cpp) copy it into the player slot (state), and the simulation only reads that copy. Sent via memcpy, so no strings.
namespace coop {

MEALKIT_STRUCT(Loadout) {
    MEALKIT_FIELD(mealkit::u32, body, 0, "영웅 몸 종류 (tables/bodies.table 행 번호)");
};

} // namespace coop
