// Confirmed command processing: at the very start of World::step(), this tick's commands in player index → send order.
// Game commands are MEALKIT_STRUCTs the view side sends with mealkit::sim::sendCommand(session, value) and reads here with c.read(value).
#include "sim/Systems.h"

#include "mealkit/sim/Commands.h"

namespace sandbox {

void applyCommands(World& world, const mealkit::net::TickInputs& inputs) {
    mealkit::sim::forEachCommand(inputs, [&](const mealkit::sim::Command& c) {
        if (mealkit::sim::Presence p; mealkit::sim::readPresence(c, p)) setPresence(world, p);
    });
}

} // namespace sandbox
