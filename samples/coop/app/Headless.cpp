#include "app/Headless.h"

#include "sim/Loadout.h"
#include "sim/Commands.h"

#include "mealkit/data/Tables.h"
#include "mealkit/game/Loadout.h"
#include "mealkit/sim/Commands.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace mealkit;

namespace coop {
namespace {

i8 quantize(f32 v) { return static_cast<i8>(std::lround(std::clamp(v, -1.0f, 1.0f) * 127.0f)); }

} // namespace

// Tables are read from files every match (per-match loading, docs/CODING_MODEL.md "Asset references and loading").
void Headless::startWorld(World& world, const game::MatchInfo& match, const Tuning& tuning,
                          const scene::Scene& flatScene, asset::ModelCache& models, u32 physicsThreads) {
    sim::Table<BodyKind> bodyKinds;
    data::loadTable(bodyKinds);
    std::vector<u32> bodies(match.playerCount, 0u); // match start data (players who didn't send one get the first body)
    for (u32 p = 0; p < match.playerCount; ++p) {
        Loadout l;
        if (game::readLoadout(match, p, l)) bodies[p] = l.body;
    }
    world.reset(match.seed, match.playerCount, tuning, bodyKinds, bodies, flatScene, models,
                reflect::tickRate(), physicsThreads); // tick rate: mealkit.project (goes into the content hash)
}

// Outside the simulation, so trig is fine (only the resulting bytes go in as input)
net::PlayerInput Headless::botInput(u32 tick, u8 player) {
    GameInput g;
    // Heads for the goal corner (+x +z in the sample scene) weaving a little, and stands still for 3 of every 12 seconds
    // (so the team bunches up, chasers catch someone, and teammates nearby revive them)
    const f32 a = static_cast<f32>(tick) * 0.05f + static_cast<f32>(player) * 1.7f;
    const bool resting = tick % 360u >= 270u;
    g.moveX = resting ? 0 : quantize(0.7f + 0.3f * std::sin(a));
    g.moveZ = resting ? 0 : quantize(0.7f + 0.3f * std::cos(a));
    if ((tick + player * 11u) % 70u == 0) g.buttons |= kButtonPush;
    if ((tick + player * 7u) % 75u == 0) g.buttons |= kButtonJump;
    net::PlayerInput pi;
    std::memcpy(pi.bytes, &g, sizeof(g));
    return pi;
}

void Headless::botCommands(u32 tick, u8 player, net::LockstepSession& session) {
    if ((tick + player * 53u) % 300u == 150u) sim::sendCommand(session, DebugStunAll{});
}

void Headless::botLoadout(u8 player, net::LockstepSession& session) {
    Loadout l;
    l.body = player % 3u; // normal, light, heavy (bodies.table order, all active ragdolls)
    game::setLoadout(session, l);
}

void Headless::defaults(game::MatchOptions& options, const ProjectSettings& project) {
    if (options.port == 0) options.port = project.defaultPort;
    if (options.maxPlayers == 0) options.maxPlayers = project.defaultMaxPlayers;
}

} // namespace coop

MEALKIT_HEADLESS_GAME(coop::Headless)
