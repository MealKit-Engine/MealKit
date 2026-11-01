#pragma once

#include "sim/World.h"

#include "mealkit/sim/Commands.h"

// sim/ system functions. World::step() calls them directly in a fixed order (World.cpp). Each-runs-its-own-Update style isn't used.
// When systems need to notify each other, they call functions directly (e.g. controlPawns → pushProps).
namespace sandbox {

// Pawns.cpp
Vec3 spawnPosition(const World& world, u32 index);
void createControllers(World& world); // match start: one per player slot, then AI
Id<Pawn> spawnPawn(World& world, const Vec3& position, f32 massScale);
const BodyKind* bodyOf(const World& world, u8 player); // a player's body kind (nullptr for AI or an empty table)
void possess(World& world, Id<Controller> controller, Id<Pawn> pawn);
void unpossess(World& world, Id<Controller> controller);
void respawnPawns(World& world);      // a new Pawn for controllers without one whose respawn wait has ended
void controlPawns(World& world);      // input/AI → velocity, jump, push
void usePads(World& world);           // launches Pawns on jump pads upward
void removeFallenPawns(World& world); // removes fallen Pawns and waits for respawn
void sensePawns(World& world);        // physics results → position, velocity, grounded
void setPresence(World& world, const mealkit::sim::Presence& presence); // connect/disconnect mid-match (engine command)

// Match.cpp
mealkit::sim::MatchRules matchRules(const Tuning& tuning); // tuning → flow rules (World::reset, setTuning)
void startMatch(World& world);         // match start: waiting/countdown (World::reset)
void updateMatch(World& world);        // phase changes (at the start of step, after commands)
bool matchPlaying(const World& world); // in play (input/AI/scoring only then)
bool slotOut(const World& world, u32 slot); // out of lives this round (no respawn)
void onPlayerFell(World& world, u32 slot);  // scoring and lives (removeFallenPawns)

// Commands.cpp
inline constexpr u32 kMaxShockwavePushes = 8; // number of pushes one debug command can do (so a bad value doesn't stall)
void applyCommands(World& world, const mealkit::net::TickInputs& inputs); // confirmed commands (at the start of step)

// Props.cpp
void spawnProps(World& world, u64 seed); // count boxes per box spawn area (scene order, same random sequence)
void pushProps(World& world, const Vec3& center);
void recoverFallenProps(World& world);
void senseProps(World& world); // physics results → position, rotation

} // namespace sandbox
