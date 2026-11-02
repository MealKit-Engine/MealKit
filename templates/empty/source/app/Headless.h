#pragma once

#include "app/ProjectSettings.h"
#include "sim/World.h"

#include "mealkit/game/Headless.h"

// Headless entry point (mealkit/game/Headless.h): functions the engine calls when running only the World without window/renderer.
// The <name>_headless executable is built from sim/ + app/Headless.cpp only. GameSession uses the same functions for match start and bot input.
namespace sandbox {

struct Headless {
    using World = sandbox::World;
    using TuningType = Tuning;
    using ProjectSettingsType = ProjectSettings;

    static void startWorld(World& world, const mealkit::game::MatchInfo& match, const Tuning& tuning,
                           const mealkit::scene::Scene& flatScene, mealkit::asset::ModelCache& models, u32 physicsThreads);
    // Scripted input (--bot): walks in a circle and jumps now and then
    static mealkit::net::PlayerInput botInput(u32 tick, u8 player);
    // Default port/player count from project settings
    static void defaults(mealkit::game::MatchOptions& options, const ProjectSettings& project);
};

} // namespace sandbox
