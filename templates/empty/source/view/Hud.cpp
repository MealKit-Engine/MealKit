#include "view/Hud.h"

using namespace mealkit;

namespace sandbox {

void fillHud(const WorldState& state, const Tuning& tuning, HudVm& vm) {
    const u32 rate = reflect::tickRate();
    const u32 left = state.flow.phaseEnd.remaining(state.tick);
    vm.countdown = state.flow.phase == MatchPhase::Countdown ? (left + rate - 1) / rate : 0;
    if (tuning.roundLength.value == 0) vm.time = static_cast<f32>(state.tick) / static_cast<f32>(rate); // endless match: elapsed time
    else if (state.flow.phase == MatchPhase::Countdown) vm.time = static_cast<f32>(tuning.roundLength.value) / static_cast<f32>(rate);
    else vm.time = state.flow.phase == MatchPhase::Playing ? static_cast<f32>(left) / static_cast<f32>(rate) : 0.0f;
}

} // namespace sandbox
