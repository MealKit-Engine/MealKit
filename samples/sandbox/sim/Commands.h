#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/core/Types.h"

// Confirmed command types (mealkit/sim/Commands.h): the view side sends them with sim::sendCommand, and applyCommands processes them at the very start of World::step().
// Sent via memcpy, so no strings/containers.
namespace sandbox {

// Debug: pushes boxes away around the sending player's character (same force as a push, no cooldown). Does nothing while waiting to respawn.
MEALKIT_STRUCT(DebugShockwave) {
    MEALKIT_FIELD(mealkit::u32, pushes, 1, "밀치기 횟수");
};

} // namespace sandbox
