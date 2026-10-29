// Confirmed command processing: at the very start of World::step(), this tick's commands in player index → send order.
#include "sim/Commands.h"
#include "sim/Systems.h"

#include "mealkit/sim/Commands.h"

#include <algorithm>

namespace sandbox {

void applyCommands(World& world, const mealkit::net::TickInputs& inputs) {
    mealkit::sim::forEachCommand(inputs, [&](const mealkit::sim::Command& c) {
        if (mealkit::sim::Presence p; mealkit::sim::readPresence(c, p)) {
            setPresence(world, p);
            return;
        }
        if (DebugShockwave d; c.read(d)) {
            const Pawn* pawn = playerPawn(world.state, c.player);
            if (!pawn) return;
            const u32 pushes = std::min(d.pushes, kMaxShockwavePushes);
            for (u32 i = 0; i < pushes; ++i) pushProps(world, pawn->position);
            world.events.push(makeEvent(EventKind::Push, c.player, pawn->position, world.tuning.pushRadius));
        }
    });
}

} // namespace sandbox
