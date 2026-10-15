#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/core/Types.h"

namespace mealkit {

// PCG32: 작고 빠르며 플랫폼과 무관하게 같은 수열을 낸다(정수 연산만 사용).
// 시뮬레이션용 난수는 반드시 이것을 쓰고, 연출용 난수와 섞지 않는다.
// 필드 선언이라 WorldState에 넣으면 난수 상태도 해시·스냅샷에 들어간다: MEALKIT_FIELD(Pcg32, rng, (Pcg32{seed}), "난수")
MEALKIT_STRUCT(Pcg32) {
    MEALKIT_FIELD(u64, state, 0, "상태");
    MEALKIT_FIELD(u64, inc, 1, "수열");

    Pcg32() : Pcg32(0x853c49e6748fea9bull) {}
    explicit Pcg32(u64 seed, u64 sequence = 0xda3e39cb94b95bdbull) {
        inc = (sequence << 1u) | 1u;
        next();
        state += seed;
        next();
    }

    u32 next() {
        const u64 old = state;
        state = old * 6364136223846793005ull + inc;
        const u32 xorShifted = static_cast<u32>(((old >> 18u) ^ old) >> 27u);
        const u32 rot = static_cast<u32>(old >> 59u);
        return (xorShifted >> rot) | (xorShifted << ((32u - rot) & 31u));
    }

    // [0, 1) — 24비트 정수를 정확히 float로 바꾸므로 결정론적이다.
    f32 next01() { return static_cast<f32>(next() >> 8) * (1.0f / 16777216.0f); }
    f32 range(f32 lo, f32 hi) { return lo + (hi - lo) * next01(); }
    u32 below(u32 bound) { return bound ? next() % bound : 0; }
};

} // namespace mealkit
