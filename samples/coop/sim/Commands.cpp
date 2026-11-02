// Confirmed command processing: at the very start of World::step(), this tick's commands in player index → send order.
#include "sim/Commands.h"
#include "sim/Systems.h"

#include "mealkit/sim/Commands.h"

namespace coop {

void applyCommands(World& world, const mealkit::net::TickInputs& inputs) {
    mealkit::sim::forEachCommand(inputs, [&](const mealkit::sim::Command& c) {
        if (mealkit::sim::Presence p; mealkit::sim::readPresence(c, p)) {
            setPresence(world, p);
            return;
        }
        if (DebugStunAll d; c.read(d)) {
            for (auto [id, pawn] : world.state.pawns) {
                if (pawn.kind != PawnKind::Chaser) continue;
                pawn.stunned.start(world.state.tick, world.tuning.shoveStun.value);
                world.events.push(makeEvent(EventKind::Stunned, kNoPlayer, pawn.position, 0.0f));
            }
        }
    });
}

} // namespace coop
