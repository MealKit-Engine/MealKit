// Match flow: the engine's mealkit::sim::advanceMatch with co-op rules (every player is side 0). The team loses when time runs out
// or everyone is down at once (elimination), and wins when all connected heroes stand up inside the goal for goalHold ticks
// (finishRound). This file calls the game's own functions on each transition.
#include "sim/Systems.h"

namespace coop {
namespace {

using mealkit::sim::FlowEvent;

// A hero counts as alive while up (or still waiting to respawn after a fall)
std::array<mealkit::sim::MatchPlayer, mealkit::session::kMaxPlayers> matchPlayers(const World& world) {
    std::array<mealkit::sim::MatchPlayer, mealkit::session::kMaxPlayers> players{};
    for (u32 i = 0; i < world.state.playerCount; ++i) {
        const Pawn* p = playerPawn(world.state, i);
        players[i].present = world.state.slots[i].connected;
        players[i].alive = !p || !p->downed;
    }
    return players;
}

// Every connected hero is up and inside the goal
bool teamAtGoal(const World& world) {
    const WorldState& s = world.state;
    bool anyone = false;
    for (u32 i = 0; i < s.playerCount; ++i) {
        if (!s.slots[i].connected) continue;
        const Pawn* p = playerPawn(s, i);
        if (!p || p->downed) return false;
        const Vec3& q = p->position;
        if (q.x < world.goalMin.x || q.x > world.goalMax.x || q.z < world.goalMin.z || q.z > world.goalMax.z ||
            q.y < world.goalMin.y || q.y > world.goalMax.y)
            return false;
        anyone = true;
    }
    return anyone;
}

// New round: everyone back at the spawn points, chasers too (the first round's Pawns come from World::reset)
void onRoundStart(World& world) {
    WorldState& s = world.state;
    s.goalTicks = 0;
    if (s.flow.round > 1) {
        for (auto [id, c] : s.controllers) {
            const Id<Pawn> pawn = c.pawn;
            unpossess(world, id);
            if (s.pawns.alive(pawn)) s.pawns.destroy(pawn); // the body is deleted at the end of the tick
            c.respawn = {};                                  // respawnPawns makes a new one this tick
            c.prey = {};
            c.aiPathCount = c.aiPathNext = 0;
        }
    }
    world.events.push(makeEvent(EventKind::MatchStart, kNoPlayer, {}, static_cast<f32>(s.flow.round)));
}

void onRoundEnd(World& world) {
    const bool won = world.state.flow.roundWinner == 0;
    world.events.push(makeEvent(EventKind::RoundEnd, kNoPlayer, {}, won ? 1.0f : 0.0f));
}

void handle(World& world, FlowEvent event) {
    switch (event) {
    case FlowEvent::RoundStart: onRoundStart(world); break;
    case FlowEvent::RoundEnd: onRoundEnd(world); break;
    case FlowEvent::MatchEnd: world.events.push(makeEvent(EventKind::MatchEnd, kNoPlayer, {}, 0.0f)); break;
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
    r.elimination = true; // everyone down at once = lost
    r.coop = true;
    return r;
}

void startMatch(World& world) {
    const auto players = matchPlayers(world);
    handle(world, mealkit::sim::startMatchFlow(world.state.flow, world.rules, world.state.tick, players));
}

void updateMatch(World& world) {
    WorldState& s = world.state;
    const auto players = matchPlayers(world);
    handle(world, mealkit::sim::advanceMatch(s.flow, world.rules, s.tick, players));
    if (s.flow.phase == MatchPhase::Countdown) { // 3, 2, 1
        const u32 left = s.flow.phaseEnd.remaining(s.tick);
        if (left > 0 && left % world.tickRate == 0)
            world.events.push(makeEvent(EventKind::Countdown, kNoPlayer, {}, static_cast<f32>(left / world.tickRate)));
    }
    if (!matchPlaying(world) || !world.hasGoal) return;
    s.goalTicks = teamAtGoal(world) ? s.goalTicks + 1 : 0;
    if (s.goalTicks >= world.tuning.goalHold.value) handle(world, mealkit::sim::finishRound(s.flow, world.rules, s.tick, 0));
}

bool matchPlaying(const World& world) { return world.state.flow.phase == MatchPhase::Playing; }

} // namespace coop
