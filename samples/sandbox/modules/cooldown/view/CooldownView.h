#pragma once

#include "cooldown/sim/Cooldown.h"

// Cooldown view values. Put into a view model and bind to widthRatio etc. in assets/ui/cooldown_bar.ui.
namespace cooldown {

// 0 (just used) .. 1 (ready)
inline mealkit::f32 readiness(const Cooldown& c, u32 tick) {
    if (c.length == 0) return 1.0f;
    const u32 left = remaining(c, tick);
    return 1.0f - static_cast<mealkit::f32>(left) / static_cast<mealkit::f32>(c.length);
}

// Remaining time (seconds, for view text)
inline mealkit::f32 secondsLeft(const Cooldown& c, u32 tick) {
    return static_cast<mealkit::f32>(remaining(c, tick)) / static_cast<mealkit::f32>(mealkit::reflect::tickRate());
}

} // namespace cooldown
