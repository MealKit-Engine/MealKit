#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/sim/TickTimer.h"

// Cooldown module (docs/index.md): reuse delay for skills/actions. The game puts the state as a field in its own struct like WorldState or Pawn.
// Functions take only this state and the tick number (they don't know the game's World). Counting only in ticks keeps it deterministic.
namespace cooldown {

using mealkit::u32;

MEALKIT_STRUCT(Cooldown) {
    MEALKIT_FIELD(mealkit::sim::TickTimer, timer, {}, "Ready again at");
    MEALKIT_FIELD(u32, length, 0, "Length of the last start (ticks)"); // for the view's fill ratio
};

inline bool ready(const Cooldown& c, u32 tick) { return c.timer.ready(tick); }
inline u32 remaining(const Cooldown& c, u32 tick) { return c.timer.remaining(tick); }

// Starts waiting again right away (regardless of whether it's ready)
inline void start(Cooldown& c, u32 tick, u32 length) {
    c.timer.start(tick, length);
    c.length = length;
}

// If ready, uses it (starts waiting again) and returns true, otherwise false
inline bool tryUse(Cooldown& c, u32 tick, u32 length) {
    if (!ready(c, tick)) return false;
    start(c, tick, length);
    return true;
}

// Makes it not wait (respawn, item effects, etc.)
inline void reset(Cooldown& c) { c = {}; }

} // namespace cooldown
