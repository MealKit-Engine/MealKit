#include "app/Headless.h"

#include "sim/Commands.h"

#include "mealkit/game/Tables.h"
#include "mealkit/sim/Commands.h"

#include <algorithm>
#include <cmath>
#include <cstring>

using namespace mealkit;

namespace sandbox {
namespace {

constexpr u32 kDefaultBoxes = 2000; // --match-data(창 있는 게임은 --bodies)가 없을 때 상자 수

i8 quantize(f32 v) { return static_cast<i8>(std::lround(std::clamp(v, -1.0f, 1.0f) * 127.0f)); }

} // namespace

// 상자 수는 씬의 상자 생성 영역이 정한다. matchData(--match-data, 창 있는 게임은 --bodies)가 0이 아니면 그 수로 맞춘다 (성능 측정).
// 표는 매치마다 파일에서 읽는다 (매치 단위 적재, docs/CODING_MODEL.md "에셋 참조와 적재").
void Headless::startWorld(World& world, const game::MatchInfo& match, const Tuning& tuning,
                          const scene::Scene& flatScene, asset::ModelCache& models, u32 physicsThreads) {
    sim::Table<PropKind> propKinds;
    game::loadTable(propKinds);
    world.reset(match.seed, match.playerCount, match.matchData, tuning, propKinds, flatScene, models, physicsThreads);
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

void Headless::botCommands(u32 tick, u8 player, net::LockstepSession& session) {
    if ((tick + player * 53u) % 300u == 150u) sim::sendCommand(session, DebugShockwave{});
}

void Headless::defaults(game::HeadlessOptions& options, const ProjectSettings& project) {
    if (options.port == 0) options.port = project.defaultPort;
    if (options.maxPlayers == 0) options.maxPlayers = project.defaultMaxPlayers;
}

} // namespace sandbox

MEALKIT_HEADLESS_GAME(sandbox::Headless)
