# Match flow

The phases of a match: waiting until enough players are in, a start countdown, rounds that end on a score limit, a time limit or elimination, best of N, and a results phase. Free for all, teams, or co-op against the game.

- Version 0.1.0, engine 0.9, requires the engine modules `sim` and `session`
- State: `matchflow::MatchFlow` (phase, phase end tick, round, winners, rounds won per side)
- Simulation: `sim/MatchFlow.h` (header only)

Until engine 0.9.8 this was `mealkit/sim/MatchFlow.h` in the engine. Games that used it change the include to `matchflow/sim/MatchFlow.h` and `mealkit::sim::` to `matchflow::` (the type and function names are the same).

## Adding

```
tools\module.ps1 add -Project <your project> matchflow
```

Put the state in your `WorldState` and the rules in your tuning:

```cpp
// source/sim/World.h
#include "matchflow/sim/MatchFlow.h"

MEALKIT_STRUCT(WorldState) {
    ...
    MEALKIT_FIELD(matchflow::MatchFlow, flow, {}, "Match flow (phase, round, winners)");
};

// source/sim/Tuning.h
MEALKIT_FIELD(mealkit::Ticks, countdown, mealkit::Ticks::seconds(3.0f), "Start countdown (s, 0 = none)");
MEALKIT_FIELD(mealkit::Ticks, roundLength, mealkit::Ticks::seconds(90.0f), "Round length (s, 0 = no limit)");
```

## Usage

Build `MatchRules` from tuning at match start, then once per tick give the flow one `MatchPlayer` per slot (present, alive, round score) and switch on what it returns:

```cpp
// source/sim/Match.cpp
void startMatch(World& world) {
    handle(world, matchflow::startMatchFlow(world.state.flow, world.rules, world.state.tick, matchPlayers(world)));
}

void updateMatch(World& world) {
    handle(world, matchflow::advanceMatch(world.state.flow, world.rules, world.state.tick, matchPlayers(world)));
}

void handle(World& world, matchflow::FlowEvent e) {
    switch (e) {
    case matchflow::FlowEvent::RoundStart: resetRound(world); break; // scores, lives, positions
    case matchflow::FlowEvent::MatchEnd: break;                     // flow.matchWinner is set
    default: break;
    }
}
```

| Function | What it does |
|---|---|
| `startMatchFlow(flow, rules, tick, players)` | Resets the flow and makes the first transition |
| `advanceMatch(flow, rules, tick, players)` | One tick: returns `CountdownStart`, `RoundStart`, `RoundEnd`, `MatchEnd` or `None` |
| `finishRound(flow, rules, tick, winner)` | Ends the round now with a winner the game decided (a co-op goal) |
| `matchSide(rules, slot)`, `matchSideCount(rules)` | Which side a slot plays for |

`MatchRules` fields: `minPlayers`, `countdown`, `roundLength`, `roundEndDelay`, `rounds`, `scoreLimit`, `elimination`, `teamCount`, `coop`. 0 turns a rule off.

On screen, read `flow.phase` and `flow.phaseEnd` (a `TickTimer`) for a countdown number or a round clock. The `screens` module opens its results screen when the game says the match reached results, usually `flow.phase == matchflow::FlowPhase::Results`.

## How it works

Transitions depend only on tick numbers and the players passed in, never on real time or callbacks, so every peer makes the same transitions on the same tick. `MatchFlow` is declared with `MEALKIT_FIELD`, so as a field of your state it is in the state hash, snapshots (rewind, late join) and the editor's [State] panel.

## Version history

- 0.1.0: first version, moved from the engine's `mealkit/sim/MatchFlow.h` (sim module 0.1.2) with the same behaviour
