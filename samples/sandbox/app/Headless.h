#pragma once

#include "app/ProjectSettings.h"
#include "sim/World.h"

#include "mealkit/game/Headless.h"

// Headless entry point (mealkit/game/Headless.h): functions the engine calls when running only the World without window/renderer.
// The sandbox_headless executable is built from sim/ + app/Headless.cpp only → view/ and renderer headers aren't used here.
// GameSession (windowed game, editor) uses the same functions for match start and bot input.
namespace sandbox {

struct Headless {
    using World = sandbox::World;
    using TuningType = Tuning;
    using ProjectSettingsType = ProjectSettings;

    // Match start: box count from the scene's box spawn areas (or matchData if not 0). Boxes don't grow mid-match, so Pool capacity = box count
    static void startWorld(World& world, const mealkit::game::MatchInfo& match, const Tuning& tuning,
                           const mealkit::scene::Scene& flatScene, mealkit::asset::ModelCache& models,
                           u32 physicsThreads);
    // Scripted input (--bot): walks in a circle and periodically pushes/jumps
    static mealkit::session::PlayerInput botInput(u32 tick, u8 player);
    // Bot confirmed commands: an occasional debug shockwave (puts the command channel into determinism tests)
    static void botCommands(u32 tick, u8 player, mealkit::session::Session& session);
    // Bot match start data: a different body per player (puts the loadout path into determinism tests)
    static void botLoadout(u8 player, mealkit::session::Session& session);
    // Default port/player count from project settings
    static void defaults(mealkit::game::MatchOptions& options, const ProjectSettings& project);
};

} // namespace sandbox
