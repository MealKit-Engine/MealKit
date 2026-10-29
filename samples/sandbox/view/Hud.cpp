#include "view/Hud.h"

#include "cooldown/view/CooldownView.h"

#include <algorithm>

using namespace mealkit;

namespace sandbox {

void fillHud(const WorldState& state, const Tuning& tuning, u8 localPlayer, const ViewTuning& look,
             const std::array<std::string, net::kMaxPlayers>& names, HudVm& vm) {
    const f32 rate = static_cast<f32>(reflect::tickRate());
    const u32 left = state.phaseEnd.remaining(state.tick);
    vm.countdown = state.phase == MatchPhase::Countdown ? (left + static_cast<u32>(rate) - 1) / static_cast<u32>(rate) : 0;
    if (tuning.matchLength.value == 0) {
        vm.time = static_cast<f32>(state.tick) / rate; // endless match: elapsed time
        vm.go = false;
    } else {
        vm.time = state.phase == MatchPhase::Countdown ? static_cast<f32>(tuning.matchLength.value) / rate
                  : state.phase == MatchPhase::Playing ? static_cast<f32>(left) / rate
                                                       : 0.0f;
        vm.go = state.phase == MatchPhase::Playing && tuning.matchLength.value - left < static_cast<u32>(rate);
    }
    vm.playersCount = std::min<u32>(state.playerCount, net::kMaxPlayers);
    for (u32 i = 0; i < vm.playersCount; ++i) {
        PlayerRowVm& row = vm.players[i];
        row.name = names[i];
        row.falls = state.slots[i].falls;
        row.you = i == localPlayer;
        row.color = look.playerColors[i];
    }
    const Pawn* pawn = playerPawn(state, localPlayer);
    vm.alive = pawn != nullptr;
    vm.pushReady = pawn ? cooldown::readiness(pawn->push, state.tick) : 1.0f;
}

} // namespace sandbox
