#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/core/Types.h"

namespace mealkit {

// PCG32: small, fast, and produces the same sequence regardless of platform (integer ops only).
// Simulation random numbers must use this, never mixed with cosmetic random numbers.
// It's a field declaration, so putting it in WorldState puts the RNG state into the hash/snapshots too: MEALKIT_FIELD(Pcg32, rng, (Pcg32{seed}), "RNG")
MEALKIT_STRUCT(Pcg32) {
    MEALKIT_FIELD(u64, state, 0, "State");
    MEALKIT_FIELD(u64, inc, 1, "Sequence");

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

    // [0, 1) — converts a 24-bit integer exactly to float, so it's deterministic.
    f32 next01() { return static_cast<f32>(next() >> 8) * (1.0f / 16777216.0f); }
    f32 range(f32 lo, f32 hi) { return lo + (hi - lo) * next01(); }
    u32 below(u32 bound) { return bound ? next() % bound : 0; }
};

} // namespace mealkit
