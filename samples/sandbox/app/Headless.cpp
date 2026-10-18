#include "app/Headless.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace mealkit;

namespace sandbox {
namespace {

constexpr u32 kDefaultBoxes = 2000; // --match-data(창 있는 게임은 --bodies)가 없을 때 상자 수

i8 quantize(f32 v) { return static_cast<i8>(std::lround(std::clamp(v, -1.0f, 1.0f) * 127.0f)); }

} // namespace

void Headless::startWorld(World& world, const game::MatchInfo& match, const Tuning& tuning,
                          const scene::Scene& flatScene, asset::ModelCache& models, u32 physicsThreads) {
    world.reset(match.seed, match.playerCount, match.matchData, match.matchData, tuning, flatScene, models,
                physicsThreads);
}

// 시뮬레이션 밖이라 삼각함수를 써도 된다 (결과 바이트만 입력으로 들어간다)
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

void Headless::defaults(game::HeadlessOptions& options, const ProjectSettings& project) {
    if (options.port == 0) options.port = project.defaultPort;
    if (options.maxPlayers == 0) options.maxPlayers = project.defaultMaxPlayers;
    if (options.matchData == 0) options.matchData = kDefaultBoxes;
}

} // namespace sandbox

MEALKIT_HEADLESS_GAME(sandbox::Headless)
