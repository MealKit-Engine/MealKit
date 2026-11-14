#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/core/Reflect.h"

#include <string>

// View models for game screen UI (mealkit/ui/Ui.h). The view side fills state → view model every frame, and assets/ui/*.ui binds by field name.
namespace sandbox {

// assets/ui/hud.ui
MEALKIT_VIEWMODEL(HudVm) {
    MEALKIT_FIELD(mealkit::f32, time, 0.0f, "지난 경기 시간 (초)");
};

// State → HUD (every frame)
void fillHud(const WorldState& state, HudVm& vm);

} // namespace sandbox
