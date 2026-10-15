#pragma once

#include "mealkit/core/Types.h"

namespace mealkit {

// PCG32: 작고 빠르며 플랫폼과 무관하게 같은 수열을 낸다(정수 연산만 사용).
// 시뮬레이션용 난수는 반드시 이것을 쓰고, 연출용 난수와 섞지 않는다.
struct Pcg32 {
    u64 state = 0;
    u64 inc = 1;

    explicit Pcg32(u64 seed = 0x853c49e6748fea9bull, u64 sequence = 0xda3e39cb94b95bdbull) {
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
