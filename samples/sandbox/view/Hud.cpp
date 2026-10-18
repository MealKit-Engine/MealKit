#include "view/Hud.h"

#include <algorithm>

using namespace mealkit;

namespace sandbox {

void fillHud(const WorldState& state, const Tuning& tuning, u8 localPlayer, const ViewTuning& look, HudVm& vm) {
    vm.time = static_cast<f32>(state.tick) / static_cast<f32>(reflect::tickRate());
    vm.playersCount = std::min<u32>(state.playerCount, net::kMaxPlayers);
    for (u32 i = 0; i < vm.playersCount; ++i) {
        PlayerRowVm& row = vm.players[i];
        row.name = "P" + std::to_string(i + 1);
        row.falls = state.slots[i].falls;
        row.you = i == localPlayer;
        row.color = look.playerColors[i];
    }
    const Pawn* pawn = playerPawn(state, localPlayer);
    vm.alive = pawn != nullptr;
    vm.pushReady = 1.0f;
    if (pawn && tuning.pushCooldown.value > 0)
        vm.pushReady = 1.0f - static_cast<f32>(pawn->pushReady.remaining(state.tick)) / static_cast<f32>(tuning.pushCooldown.value);
}

} // namespace sandbox
