// Match flow: the engine's mealkit::sim::advanceMatch decides phases (waiting → countdown → playing → round end → results)
// from tuning rules and the PlayerSlot scoreboard; this file calls the game's own functions on each transition.
// All peers reach the same phase on the same tick (state field). The screen flow (app/) only reads the phase.
#include "sim/Systems.h"

namespace sandbox {
namespace {

using mealkit::sim::FlowEvent;

std::array<mealkit::sim::MatchPlayer, mealkit::session::kMaxPlayers> matchPlayers(const World& world) {
    std::array<mealkit::sim::MatchPlayer, mealkit::session::kMaxPlayers> players{};
    for (u32 i = 0; i < world.state.playerCount; ++i) {
        const PlayerSlot& slot = world.state.slots[i];
        players[i].present = slot.connected;
        players[i].alive = !slotOut(world, i);
        players[i].score = slot.score;
    }
    return players;
}

// New round: clear round scores, refill lives, and put everyone back at the spawn points (the first round's Pawns come from World::reset)
void onRoundStart(World& world) {
    WorldState& s = world.state;
    for (u32 i = 0; i < s.playerCount; ++i) {
        s.slots[i].score = 0;
        s.slots[i].lives = world.tuning.lives;
    }
    if (s.flow.round > 1) {
        for (auto [id, c] : s.controllers) {
            const Id<Pawn> pawn = c.pawn;
            unpossess(world, id);
            if (s.pawns.alive(pawn)) s.pawns.destroy(pawn); // the body is deleted at the end of the tick
            c.respawn = {};                                  // respawnPawns makes a new one this tick
        }
    }
    world.events.push(makeEvent(EventKind::MatchStart, kNoPlayer, {}, static_cast<f32>(s.flow.round)));
}

void onRoundEnd(World& world) {
    const u32 winner = world.state.flow.roundWinner;
    world.events.push(makeEvent(EventKind::RoundEnd, kNoPlayer, {}, winner == mealkit::sim::kNoSide ? -1.0f : static_cast<f32>(winner)));
}

void onMatchEnd(World& world) { world.events.push(makeEvent(EventKind::MatchEnd, kNoPlayer, {}, 0.0f)); }

void handle(World& world, FlowEvent event) {
    switch (event) {
    case FlowEvent::RoundStart: onRoundStart(world); break;
    case FlowEvent::RoundEnd: onRoundEnd(world); break;
    case FlowEvent::MatchEnd: onMatchEnd(world); break;
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
    r.scoreLimit = t.scoreLimit;
    r.elimination = t.lives > 0;
    r.teamCount = t.teams;
    return r;
}

void startMatch(World& world) {
    WorldState& s = world.state;
    for (u32 i = 0; i < s.playerCount; ++i) s.slots[i].lives = world.tuning.lives;
    const auto players = matchPlayers(world);
    handle(world, mealkit::sim::startMatchFlow(s.flow, world.rules, s.tick, players));
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
}

bool matchPlaying(const World& world) { return world.state.flow.phase == MatchPhase::Playing; }

bool slotOut(const World& world, u32 slot) { return world.tuning.lives > 0 && world.state.slots[slot].lives == 0; }

// A player fell during play: one point to every connected player of another side, one life lost
void onPlayerFell(World& world, u32 slot) {
    WorldState& s = world.state;
    ++s.slots[slot].falls;
    const u32 side = mealkit::sim::matchSide(world.rules, slot);
    for (u32 i = 0; i < s.playerCount; ++i)
        if (s.slots[i].connected && mealkit::sim::matchSide(world.rules, i) != side) ++s.slots[i].score;
    if (s.slots[slot].lives > 0) --s.slots[slot].lives;
}

} // namespace sandbox
