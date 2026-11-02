#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/core/Types.h"

// Confirmed command types (mealkit/sim/Commands.h): the view side sends them with sim::sendCommand, and applyCommands processes them at the very start of World::step().
namespace coop {

// Debug: stuns every chaser for the shove stun time
MEALKIT_STRUCT(DebugStunAll) {
    MEALKIT_FIELD(mealkit::u32, unused, 0, "-");
};

} // namespace coop
