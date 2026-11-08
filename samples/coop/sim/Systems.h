#pragma once

#include "sim/World.h"

#include "mealkit/sim/Commands.h"

// sim/ system functions. World::step() calls them directly in a fixed order (World.cpp).
namespace coop {

// Pawns.cpp
void createControllers(World& world); // match start: one per player slot, then chasers per ChaserSpawn
void possess(World& world, Id<Controller> controller, Id<Pawn> pawn);
void unpossess(World& world, Id<Controller> controller);
void respawnPawns(World& world);      // a new Pawn for controllers without one whose respawn wait has ended
void controlPawns(World& world);      // input/AI → movement, shoves
void catchHeroes(World& world);       // chasers next to a hero knock them down
void reviveHeroes(World& world);      // teammates standing next to a downed hero lift them up
void removeFallenPawns(World& world); // below the match area: back to a spawn point after a wait
void sensePawns(World& world);        // physics results → position, velocity, grounded
void setPresence(World& world, const mealkit::sim::Presence& presence); // connect/disconnect mid-match (engine command)

// Match.cpp
mealkit::sim::MatchRules matchRules(const Tuning& tuning);
void startMatch(World& world);
void updateMatch(World& world); // phase changes (start of step) and the goal check
bool matchPlaying(const World& world);

// Commands.cpp
void applyCommands(World& world, const mealkit::session::TickInputs& inputs);

} // namespace coop
