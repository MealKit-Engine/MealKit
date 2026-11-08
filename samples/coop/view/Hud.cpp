#include "view/Hud.h"

#include <algorithm>
#include <cmath>

using namespace mealkit;

namespace coop {

void fillHud(const World& world, u8 localPlayer, const ViewTuning& look,
             const std::array<std::string, session::kMaxPlayers>& names, HudVm& vm) {
    const WorldState& state = world.state;
    const Tuning& tuning = world.tuning;
    const f32 rate = static_cast<f32>(reflect::tickRate());
    const u32 left = state.flow.phaseEnd.remaining(state.tick);
    vm.countdown = state.flow.phase == MatchPhase::Countdown ? (left + static_cast<u32>(rate) - 1) / static_cast<u32>(rate) : 0;
    if (tuning.roundLength.value == 0) {
        vm.time = static_cast<f32>(state.tick) / rate; // endless match: elapsed time
        vm.go = false;
    } else {
        vm.time = state.flow.phase == MatchPhase::Countdown ? static_cast<f32>(tuning.roundLength.value) / rate
                  : state.flow.phase == MatchPhase::Playing ? static_cast<f32>(left) / rate
                                                       : 0.0f;
        vm.go = state.flow.phase == MatchPhase::Playing && tuning.roundLength.value - left < static_cast<u32>(rate);
    }
    vm.playersCount = std::min<u32>(state.playerCount, session::kMaxPlayers);
    for (u32 i = 0; i < vm.playersCount; ++i) {
        PlayerRowVm& row = vm.players[i];
        row.name = names[i];
        row.falls = state.slots[i].downs;
        row.you = i == localPlayer;
        row.color = look.playerColors[i];
    }
    const Pawn* pawn = playerPawn(state, localPlayer);
    vm.alive = pawn != nullptr;
    const u32 cooldown = tuning.shoveCooldown.value;
    vm.pushReady = pawn && cooldown > 0 ? 1.0f - static_cast<f32>(pawn->shove.remaining(state.tick)) / static_cast<f32>(cooldown) : 1.0f;
    vm.downed = pawn && pawn->downed;
    vm.revive = pawn ? static_cast<u32>(reviveProgress(world, *pawn) * 100.0f) : 0u;
    vm.showGoal = pawn && world.hasGoal && state.flow.phase == MatchPhase::Playing;
    if (vm.showGoal) {
        const Vec3 goal = (world.goalMin + world.goalMax) * 0.5f;
        const f32 dx = goal.x - pawn->position.x, dz = goal.z - pawn->position.z;
        vm.goalMeters = static_cast<u32>(std::sqrt(dx * dx + dz * dz));
    }
    const bool over = state.flow.phase == MatchPhase::RoundEnd || state.flow.phase == MatchPhase::Results;
    vm.won = over && state.flow.roundWinner == 0;
    vm.lost = over && !vm.won;
}

} // namespace coop
