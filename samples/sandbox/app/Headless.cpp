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

namespace sandbox {
namespace {

constexpr u32 kDefaultBoxes = 2000; // box count without --match-data (--bodies for windowed games)

i8 quantize(f32 v) { return static_cast<i8>(std::lround(std::clamp(v, -1.0f, 1.0f) * 127.0f)); }

} // namespace

// Box count is decided by the scene's box spawn areas. If matchData (--match-data, --bodies for windowed games) isn't 0, it's set to that count (performance measurement).
// Tables are read from files every match (per-match loading, docs/CODING_MODEL.md "Asset references and loading").
void Headless::startWorld(World& world, const game::MatchInfo& match, const Tuning& tuning,
                          const scene::Scene& flatScene, asset::ModelCache& models, u32 physicsThreads) {
    sim::Table<PropKind> propKinds;
    data::loadTable(propKinds);
    sim::Table<BodyKind> bodyKinds;
    data::loadTable(bodyKinds);
    std::vector<u32> bodies(match.playerCount, 0u); // match start data (players who didn't send one get the first body)
    for (u32 p = 0; p < match.playerCount; ++p) {
        Loadout l;
        if (game::readLoadout(match, p, l)) bodies[p] = l.body;
    }
    world.reset(match.seed, match.playerCount, match.matchData, tuning, propKinds, bodyKinds, bodies, flatScene, models,
                reflect::tickRate(), physicsThreads); // tick rate: mealkit.project (goes into the content hash)
}

// Outside the simulation, so trig is fine (only the resulting bytes go in as input)
net::PlayerInput Headless::botInput(u32 tick, u8 player) {
    GameInput g;
    const f32 a = static_cast<f32>(tick) * 0.05f + static_cast<f32>(player) * 1.7f;
    g.moveX = quantize(std::sin(a));
    g.moveZ = quantize(std::cos(a));
    if ((tick + player * 11u) % 40u == 0) g.buttons |= kButtonPush;
    if ((tick + player * 7u) % 75u == 0) g.buttons |= kButtonJump;
    net::PlayerInput pi;
    std::memcpy(pi.bytes, &g, sizeof(g));
    return pi;
}

void Headless::botCommands(u32 tick, u8 player, net::LockstepSession& session) {
    if ((tick + player * 53u) % 300u == 150u) sim::sendCommand(session, DebugShockwave{});
}

void Headless::botLoadout(u8 player, net::LockstepSession& session) {
    Loadout l;
    l.body = player % 3u; // normal, light, heavy (bodies.table order)
    game::setLoadout(session, l);
}

void Headless::defaults(game::MatchOptions& options, const ProjectSettings& project) {
    if (options.port == 0) options.port = project.defaultPort;
    if (options.maxPlayers == 0) options.maxPlayers = project.defaultMaxPlayers;
}

} // namespace sandbox

MEALKIT_HEADLESS_GAME(sandbox::Headless)
