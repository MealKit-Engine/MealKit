#include "app/Headless.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace mealkit;

namespace sandbox {
namespace {

i8 quantize(f32 v) { return static_cast<i8>(std::lround(std::clamp(v, -1.0f, 1.0f) * 127.0f)); }

} // namespace

void Headless::startWorld(World& world, const game::MatchInfo& match, const Tuning& tuning, const scene::Scene& flatScene,
                          asset::ModelCache& models, u32 physicsThreads) {
    world.reset(match.seed, match.playerCount, tuning, flatScene, models, reflect::tickRate(), physicsThreads);
}

// Outside the simulation, so trig is fine (only the resulting bytes go in as input)
net::PlayerInput Headless::botInput(u32 tick, u8 player) {
    GameInput g;
    const f32 a = static_cast<f32>(tick) * 0.05f + static_cast<f32>(player) * 1.7f;
    g.moveX = quantize(std::sin(a));
    g.moveZ = quantize(std::cos(a));
    if ((tick + player * 7u) % 75u == 0) g.buttons |= kButtonJump;
    net::PlayerInput pi;
    std::memcpy(pi.bytes, &g, sizeof(g));
    return pi;
}

void Headless::defaults(game::MatchOptions& options, const ProjectSettings& project) {
    if (options.port == 0) options.port = project.defaultPort;
    if (options.maxPlayers == 0) options.maxPlayers = project.defaultMaxPlayers;
}

} // namespace sandbox

MEALKIT_HEADLESS_GAME(sandbox::Headless)
