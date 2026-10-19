// 매치 단계: 카운트다운 → 경기 → 끝 (결과). 모든 피어가 같은 틱에 같은 단계가 된다 (상태 필드).
// 화면 흐름(app/)은 단계를 읽기만 한다: 카운트다운 숫자, 남은 시간, 끝나면 결과 화면.
#include "sim/Systems.h"

namespace sandbox {

void startMatch(World& world) {
    WorldState& s = world.state;
    s.phase = MatchPhase::Countdown;
    s.phaseEnd.start(s.tick, world.tuning.countdown);
    if (world.tuning.countdown.value == 0) s.phase = MatchPhase::Playing; // 카운트다운 없음
    if (s.phase == MatchPhase::Playing && world.tuning.matchLength.value > 0) s.phaseEnd.start(s.tick, world.tuning.matchLength);
}

void updateMatch(World& world) {
    WorldState& s = world.state;
    const u32 ticksPerSecond = static_cast<u32>(kTickRate);
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
