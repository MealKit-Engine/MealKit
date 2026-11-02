// Match flow: the engine's mealkit::sim::advanceMatch decides phases (waiting → countdown → playing → round end → results)
// from tuning rules and the PlayerSlot scores; this file calls the game's own functions on each transition.
// All peers reach the same phase on the same tick (state field). The screen flow (app/) only reads the phase.
#include "sim/Systems.h"

namespace sandbox {
namespace {

using mealkit::sim::FlowEvent;

std::array<mealkit::sim::MatchPlayer, mealkit::net::kMaxPlayers> matchPlayers(const World& world) {
    std::array<mealkit::sim::MatchPlayer, mealkit::net::kMaxPlayers> players{};
    for (u32 i = 0; i < world.state.playerCount; ++i) {
        players[i].present = world.state.slots[i].connected;
        players[i].alive = true;
        players[i].score = world.state.slots[i].score;
    }
    return players;
}

void handle(World& world, FlowEvent event) {
    switch (event) {
    case FlowEvent::RoundStart: world.events.push(makeEvent(EventKind::MatchStart, kNoPlayer, {}, 0.0f)); break;
    case FlowEvent::MatchEnd: world.events.push(makeEvent(EventKind::MatchEnd, kNoPlayer, {}, 0.0f)); break;
    case FlowEvent::RoundEnd:
    case FlowEvent::CountdownStart:
    case FlowEvent::None: break;
    }
}

} // namespace

mealkit::sim::MatchRules matchRules(const Tuning& t) {
    mealkit::sim::MatchRules r;
    r.minPlayers = t.minPlayers;
    r.countdown = t.countdown.value;
    r.roundLength = t.roundLength.value;
    r.roundEndDelay = t.roundEndDelay.value;
    r.rounds = t.rounds;
    return r;
}

void startMatch(World& world) {
    handle(world, mealkit::sim::startMatchFlow(world.state.flow, world.rules, world.state.tick, matchPlayers(world)));
}

void updateMatch(World& world) {
    WorldState& s = world.state;
    handle(world, mealkit::sim::advanceMatch(s.flow, world.rules, s.tick, matchPlayers(world)));
    if (s.flow.phase == MatchPhase::Countdown) { // 3, 2, 1
        const u32 left = s.flow.phaseEnd.remaining(s.tick);
        if (left > 0 && left % world.tickRate == 0)
            world.events.push(makeEvent(EventKind::Countdown, kNoPlayer, {}, static_cast<f32>(left / world.tickRate)));
    }
}

bool matchPlaying(const World& world) { return world.state.flow.phase == MatchPhase::Playing; }

} // namespace sandbox
