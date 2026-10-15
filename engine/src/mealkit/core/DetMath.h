#pragma once

#include "mealkit/core/Math.h"

// 결정론 수학: 시뮬레이션(sim/)에서 쓰는 삼각함수. 사칙연산, 정수 변환, std::sqrt만 쓴다.
// CRT sin/cos/atan2는 CPU·CRT 버전마다 결과 비트가 다를 수 있지만, 이 함수들은 IEEE 754 기본 연산만 쓰므로
// /fp:precise(FMA 축약 없음) 아래에서 어느 PC에서나 같은 비트를 낸다.
// 정확도: CRT와의 차이 sin·cos 6e-8, atan2 2.4e-7 (|x| <= 100 라디안, engine/tests에서 확인). 각도는 |x| < 2^14 라디안까지.
// 다항식 계수는 Cephes 라이브러리(sinf, cosf, atanf)의 최소최대 근사.
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

// x를 (pi/2)의 정수 배 k와 나머지 r(|r| <= pi/4)로 나눈다. pi/2를 두 부분으로 나눠 빼서 나머지의 정밀도를 지킨다.
inline f32 detReduce(f32 x, i32& quadrant) {
    const f32 t = x * 0.636619772367581f; // 2/pi
    const i32 k = static_cast<i32>(t >= 0.0f ? t + 0.5f : t - 0.5f);
    const f32 kf = static_cast<f32>(k);
    quadrant = k & 3;
    return (x - kf * 1.5703125f) - kf * 4.83826794897e-4f; // 1.5703125 + 4.838e-4 = pi/2
}

// |x| <= 1 이 아니어도 된다: 0 <= x 에서 atan(x)
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
    return base + ((((8.05374449538e-2f * z - 1.38776856032e-1f) * z + 1.99777106478e-1f) * z - 3.33329491539e-1f) * z * x + x);
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

// CRT atan2와 같은 규약: 결과는 [-pi, pi], (0, 0)이면 0.
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

// 시뮬레이션용 quatFromAxisAngle
inline Quat detQuatFromAxisAngle(Vec3 axis, f32 angle) {
    const Vec3 a = normalize(axis);
    const f32 s = detSin(angle * 0.5f);
    return {a.x * s, a.y * s, a.z * s, detCos(angle * 0.5f)};
}

// y축 회전 (캐릭터가 바라보는 방향). yaw = 0이면 +z, yaw가 늘면 +x 쪽으로 돈다 (atan2(x, z)와 짝).
inline Quat detYawQuat(f32 yaw) { return detQuatFromAxisAngle({0.0f, 1.0f, 0.0f}, yaw); }

} // namespace mealkit
