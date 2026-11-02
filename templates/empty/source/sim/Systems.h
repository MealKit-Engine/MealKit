#pragma once

#include "sim/World.h"

#include "mealkit/sim/Commands.h"

// sim/ system functions. World::step() calls them directly in a fixed order (World.cpp).
// When systems need to notify each other, they call functions directly. New systems go in their own file and are declared here.
namespace sandbox {

// Players.cpp
void respawnPawns(World& world);      // a new character for connected players whose respawn wait has ended
void movePawns(World& world);         // input → movement mode (velocity, jump)
void removeFallenPawns(World& world); // below tuning.fallHeight: removed, respawn wait
void sensePawns(World& world);        // physics results → position, velocity
void setPresence(World& world, const mealkit::sim::Presence& presence); // connect/disconnect mid-match (engine command)

// Match.cpp
mealkit::sim::MatchRules matchRules(const Tuning& tuning); // tuning → flow rules (World::reset, setTuning)
void startMatch(World& world);         // match start: waiting/countdown (World::reset)
void updateMatch(World& world);        // phase changes (at the start of step, after commands)
bool matchPlaying(const World& world); // in play (input only then)

// Commands.cpp
void applyCommands(World& world, const mealkit::net::TickInputs& inputs); // confirmed commands (at the start of step)

} // namespace sandbox
