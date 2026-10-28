#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/sim/TickTimer.h"

// 쿨타임 모듈 (docs/index.md): 스킬·행동의 재사용 대기. 상태는 게임이 WorldState나 Pawn 같은 자기 구조체에 필드로 넣는다.
// 함수는 이 상태와 틱 번호만 받는다(게임의 World를 모른다). 틱 번호로만 세므로 결정론이다.
namespace cooldown {

using mealkit::u32;

MEALKIT_STRUCT(Cooldown) {
    MEALKIT_FIELD(mealkit::sim::TickTimer, timer, {}, "Ready again at");
    MEALKIT_FIELD(u32, length, 0, "Length of the last start (ticks)"); // 화면의 채우기 비율용
};

inline bool ready(const Cooldown& c, u32 tick) { return c.timer.ready(tick); }
inline u32 remaining(const Cooldown& c, u32 tick) { return c.timer.remaining(tick); }

// 바로 다시 기다리게 한다 (준비됐는지와 무관)
inline void start(Cooldown& c, u32 tick, u32 length) {
    c.timer.start(tick, length);
    c.length = length;
}

// 준비됐으면 쓰고(다시 기다리기 시작) true, 아니면 false
inline bool tryUse(Cooldown& c, u32 tick, u32 length) {
    if (!ready(c, tick)) return false;
    start(c, tick, length);
    return true;
}

// 기다리지 않게 한다 (리스폰, 아이템 효과 등)
inline void reset(Cooldown& c) { c = {}; }

} // namespace cooldown
