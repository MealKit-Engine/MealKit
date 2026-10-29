// Match phases: countdown → playing → finished (results). All peers reach the same phase on the same tick (state field).
// The screen flow (app/) only reads the phase: countdown number, remaining time, results screen when finished.
#include "sim/Systems.h"

namespace sandbox {

void startMatch(World& world) {
    WorldState& s = world.state;
    s.phase = MatchPhase::Countdown;
    s.phaseEnd.start(s.tick, world.tuning.countdown);
    if (world.tuning.countdown.value == 0) s.phase = MatchPhase::Playing; // no countdown
    if (s.phase == MatchPhase::Playing && world.tuning.matchLength.value > 0) s.phaseEnd.start(s.tick, world.tuning.matchLength);
}

void updateMatch(World& world) {
    WorldState& s = world.state;
    const u32 ticksPerSecond = world.tickRate;
    switch (s.phase) {
    case MatchPhase::Countdown:
        if (s.phaseEnd.ready(s.tick)) {
            s.phase = MatchPhase::Playing;
            if (world.tuning.matchLength.value > 0) s.phaseEnd.start(s.tick, world.tuning.matchLength);
            world.events.push(makeEvent(EventKind::MatchStart, kNoPlayer, {}, 0.0f));
        } else if (s.phaseEnd.remaining(s.tick) % ticksPerSecond == 0) { // 3, 2, 1
            world.events.push(makeEvent(EventKind::Countdown, kNoPlayer, {}, static_cast<f32>(s.phaseEnd.remaining(s.tick) / ticksPerSecond)));
        }
        break;
    case MatchPhase::Playing:
        if (world.tuning.matchLength.value > 0 && s.phaseEnd.ready(s.tick)) {
            s.phase = MatchPhase::Finished;
            world.events.push(makeEvent(EventKind::MatchEnd, kNoPlayer, {}, 0.0f));
        }
        break;
    case MatchPhase::Finished: break;
    }
}

bool matchPlaying(const World& world) { return world.state.phase == MatchPhase::Playing; }

} // namespace sandbox
