#pragma once

#include "mealkit/core/Math.h"

// Deterministic math: trig functions used in the simulation (sim/). Uses only arithmetic, integer conversion, and std::sqrt.
// CRT sin/cos/atan2 can give different result bits per CPU/CRT version, but these functions use only basic IEEE 754 operations, so
// under /fp:precise (no FMA contraction) they produce the same bits on any PC.
// Accuracy: difference from the CRT sin/cos 6e-8, atan2 2.4e-7 (|x| <= 100 radians, verified in engine/tests). Angles up to |x| < 2^14 radians.
// Polynomial coefficients are minimax approximations from the Cephes library (sinf, cosf, atanf).
namespace mealkit {

inline constexpr f32 kDetPi = 3.14159265358979f;
inline constexpr f32 kDetHalfPi = 1.57079632679490f;

namespace detail {

// |r| <= pi/4
inline f32 detSinPoly(f32 r) {
    const f32 z = r * r;
    return ((-1.9515295891e-4f * z + 8.3321608736e-3f) * z - 1.6666654611e-1f) * z * r + r;
}

inline f32 detCosPoly(f32 r) {
    const f32 z = r * r;
    return ((2.443315711809948e-5f * z - 1.388731625493765e-3f) * z + 4.166664568298827e-2f) * z * z - 0.5f * z + 1.0f;
}

// Splits x into an integer multiple k of (pi/2) and a remainder r (|r| <= pi/4). pi/2 is subtracted in two parts to keep the remainder precise.
inline f32 detReduce(f32 x, i32& quadrant) {
    const f32 t = x * 0.636619772367581f; // 2/pi
    const i32 k = static_cast<i32>(t >= 0.0f ? t + 0.5f : t - 0.5f);
    const f32 kf = static_cast<f32>(k);
    quadrant = k & 3;
    return (x - kf * 1.5703125f) - kf * 4.83826794897e-4f; // 1.5703125 + 4.838e-4 = pi/2
}

// Doesn't need |x| <= 1: atan(x) for 0 <= x
inline f32 detAtanPositive(f32 x) {
    f32 base = 0.0f;
    if (x > 2.414213562373095f) { // tan(3pi/8)
        base = kDetHalfPi;
        x = -1.0f / x;
    } else if (x > 0.4142135623730950f) { // tan(pi/8)
        base = 0.785398163397448f;
        x = (x - 1.0f) / (x + 1.0f);
    }
    const f32 z = x * x;
    return base +
           ((((8.05374449538e-2f * z - 1.38776856032e-1f) * z + 1.99777106478e-1f) * z - 3.33329491539e-1f) * z * x +
            x);
}

} // namespace detail

inline f32 detSin(f32 x) {
    i32 q = 0;
    const f32 r = detail::detReduce(x, q);
    switch (q) {
    case 0: return detail::detSinPoly(r);
    case 1: return detail::detCosPoly(r);
    case 2: return -detail::detSinPoly(r);
    default: return -detail::detCosPoly(r);
    }
}

inline f32 detCos(f32 x) {
    i32 q = 0;
    const f32 r = detail::detReduce(x, q);
    switch (q) {
    case 0: return detail::detCosPoly(r);
    case 1: return -detail::detSinPoly(r);
    case 2: return -detail::detCosPoly(r);
    default: return detail::detSinPoly(r);
    }
}

inline f32 detAtan(f32 x) { return x < 0.0f ? -detail::detAtanPositive(-x) : detail::detAtanPositive(x); }

// Same convention as CRT atan2: result in [-pi, pi], 0 for (0, 0).
inline f32 detAtan2(f32 y, f32 x) {
    if (x == 0.0f) {
        if (y > 0.0f) return kDetHalfPi;
        if (y < 0.0f) return -kDetHalfPi;
        return 0.0f;
    }
    const f32 a = detAtan(y / x);
    if (x > 0.0f) return a;
    return y >= 0.0f ? a + kDetPi : a - kDetPi;
}

// quatFromAxisAngle for the simulation
inline Quat detQuatFromAxisAngle(Vec3 axis, f32 angle) {
    const Vec3 a = normalize(axis);
    const f32 s = detSin(angle * 0.5f);
    return {a.x * s, a.y * s, a.z * s, detCos(angle * 0.5f)};
}

// y-axis rotation (the direction a character faces). yaw = 0 is +z, increasing yaw turns toward +x (pairs with atan2(x, z)).
inline Quat detYawQuat(f32 yaw) { return detQuatFromAxisAngle({0.0f, 1.0f, 0.0f}, yaw); }

} // namespace mealkit
