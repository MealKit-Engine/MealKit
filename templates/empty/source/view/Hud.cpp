#include "view/Hud.h"

using namespace mealkit;

namespace sandbox {

void fillHud(const WorldState& state, HudVm& vm) {
    vm.time = static_cast<f32>(state.tick) / static_cast<f32>(reflect::tickRate());
}

} // namespace sandbox
