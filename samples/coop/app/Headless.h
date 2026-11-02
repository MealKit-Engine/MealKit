#pragma once

#include "app/ProjectSettings.h"
#include "sim/World.h"

#include "mealkit/game/Headless.h"

// Headless entry point (mealkit/game/Headless.h): functions the engine calls when running only the World without window/renderer.
// The coop_headless executable is built from sim/ + app/Headless.cpp only → view/ and renderer headers aren't used here.
// GameSession (windowed game, editor) uses the same functions for match start and bot input.
namespace coop {

struct Headless {
    using World = coop::World;
    using TuningType = Tuning;
    using ProjectSettingsType = ProjectSettings;

    // Match start: hero bodies from the loadouts, chasers from the scene's ChaserSpawn objects
    static void startWorld(World& world, const mealkit::game::MatchInfo& match, const Tuning& tuning,
                           const mealkit::scene::Scene& flatScene, mealkit::asset::ModelCache& models,
                           u32 physicsThreads);
    // Scripted input (--bot): wanders in a circle and periodically shoves/jumps
    static mealkit::net::PlayerInput botInput(u32 tick, u8 player);
    // Bot confirmed commands: an occasional debug stun (puts the command channel into determinism tests)
    static void botCommands(u32 tick, u8 player, mealkit::net::LockstepSession& session);
    // Bot match start data: a different body per player (puts the loadout path into determinism tests)
    static void botLoadout(u8 player, mealkit::net::LockstepSession& session);
    // Default port/player count from project settings
    static void defaults(mealkit::game::MatchOptions& options, const ProjectSettings& project);
};

} // namespace coop
