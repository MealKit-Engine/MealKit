# Cooldowns

Tick-based cooldowns for skills and actions: "push can be used again 0.5 s after the last push". Includes a fill bar `.ui` component for the HUD.

- Version 0.1.0, engine 0.5, no required modules
- State: `cooldown::Cooldown` (two `u32` fields)
- Simulation: `sim/Cooldown.h`, screen: `view/CooldownView.h`, UI: `modules/cooldown/ui/cooldown_bar.ui`

## Adding

```
tools\module.ps1 add -Project <your project> cooldown
```

Then connect it to your game. A cooldown belongs to something in your simulation state, usually a pawn:

```cpp
// source/sim/World.h
#include "cooldown/sim/Cooldown.h"

MEALKIT_STRUCT(Pawn) {
    ...
    MEALKIT_FIELD(cooldown::Cooldown, push, {}, "Push cooldown");
};
```

Keep the length in your tuning so designers can change it (`game.tuning`):

```cpp
// source/sim/Tuning.h
MEALKIT_FIELD(mealkit::Ticks, pushCooldown, mealkit::Ticks::seconds(0.5f), "Push cooldown (s)");
```

## Usage

| Function | What it does |
|---|---|
| `tryUse(c, tick, length)` | If ready, starts waiting `length` ticks and returns `true`. Otherwise returns `false` |
| `start(c, tick, length)` | Starts waiting now, ready or not |
| `ready(c, tick)`, `remaining(c, tick)` | Whether it can be used, ticks left |
| `reset(c)` | Ready right away (respawn, item effects) |
| `readiness(c, tick)` (view) | 0 just used .. 1 ready, for a fill bar |
| `secondsLeft(c, tick)` (view) | Seconds left, for a label |

```cpp
// source/sim/Pawns.cpp
if (intent.push && cooldown::tryUse(p.push, world.state.tick, t.pushCooldown.value)) {
    // push
}
```

On screen, put the readiness in your view model and bind the bar to it:

```cpp
// source/view/Hud.cpp
vm.pushReady = pawn ? cooldown::readiness(pawn->push, state.tick) : 1.0f;
```

```
# assets/ui/hud.ui
use pushBar modules/cooldown/ui/cooldown_bar.ui
  size fill 14
  set fill widthRatio pushReady
end
```

To restyle the bar, copy `cooldown_bar.ui` into your `assets/ui/` and use your copy. The module file then stays unmodified, so updates stay easy.

## How it works

A cooldown stores the tick at which it is ready again (`mealkit::sim::TickTimer`) and the length of the last start. It never counts down every tick and never uses real time. Every peer computes the same tick numbers, so the result is deterministic in lockstep.

Both fields are declared with `MEALKIT_FIELD`. When the cooldown is a field of your simulation state, it is part of the state hash (desync detection), snapshots (rewind, late join) and the editor's [State] panel automatically.

`length` only feeds `readiness`. Changing the tuning value in the middle of a cooldown does not change the bar of the cooldown already running.

## Version history

- 0.1.0: first version (`Cooldown`, `tryUse`, `start`, `ready`, `remaining`, `reset`, `readiness`, `secondsLeft`, `cooldown_bar.ui`)
