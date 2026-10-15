#pragma once

#include "mealkit/core/Types.h"

#include <cmath>

// 행 벡터 규약(v * M), 왼손 좌표계(x 오른쪽, y 위, z 앞).
// 시뮬레이션 코드에서는 사칙연산과 sqrt만 쓴다. sin/cos 등 CRT 함수는 CPU마다
// 다른 코드 경로(FMA 등)를 탈 수 있어 결정론을 깨므로 렌더링/입력 쪽에서만 쓴다.
namespace mealkit {

struct Vec3 {
    f32 x = 0.0f, y = 0.0f, z = 0.0f;

    constexpr Vec3() = default;
    constexpr Vec3(f32 x_, f32 y_, f32 z_) : x(x_), y(y_), z(z_) {}

    constexpr Vec3 operator+(Vec3 b) const { return {x + b.x, y + b.y, z + b.z}; }
    constexpr Vec3 operator-(Vec3 b) const { return {x - b.x, y - b.y, z - b.z}; }
    constexpr Vec3 operator*(f32 s) const { return {x * s, y * s, z * s}; }
    constexpr Vec3 operator/(f32 s) const { return {x / s, y / s, z / s}; }
    constexpr Vec3 operator-() const { return {-x, -y, -z}; }
    constexpr Vec3& operator+=(Vec3 b) { x += b.x; y += b.y; z += b.z; return *this; }
    constexpr Vec3& operator-=(Vec3 b) { x -= b.x; y -= b.y; z -= b.z; return *this; }
    constexpr Vec3& operator*=(f32 s) { x *= s; y *= s; z *= s; return *this; }
};

constexpr Vec3 operator*(f32 s, Vec3 v) { return v * s; }
constexpr f32 dot(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
constexpr Vec3 cross(Vec3 a, Vec3 b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
constexpr f32 lengthSq(Vec3 v) { return dot(v, v); }
inline f32 length(Vec3 v) { return std::sqrt(lengthSq(v)); }
inline Vec3 normalize(Vec3 v) {
    const f32 len = length(v);
    return len > 1e-12f ? v / len : Vec3{};
}
constexpr Vec3 lerp(Vec3 a, Vec3 b, f32 t) { return a + (b - a) * t; }
constexpr f32 lerp(f32 a, f32 b, f32 t) { return a + (b - a) * t; }

struct Quat {
    f32 x = 0.0f, y = 0.0f, z = 0.0f, w = 1.0f;
};

// 해밀턴 곱: (a * b)로 회전하면 b를 먼저, a를 나중에 적용한다.
constexpr Quat operator*(Quat a, Quat b) {
    return {a.w * b.x + a.x * b.w + a.y * b.z - a.z * b.y,
            a.w * b.y - a.x * b.z + a.y * b.w + a.z * b.x,
            a.w * b.z + a.x * b.y - a.y * b.x + a.z * b.w,
            a.w * b.w - a.x * b.x - a.y * b.y - a.z * b.z};
}

inline Quat normalize(Quat q) {
    const f32 len = std::sqrt(q.x * q.x + q.y * q.y + q.z * q.z + q.w * q.w);
    if (len < 1e-12f) return {};
    const f32 inv = 1.0f / len;
    return {q.x * inv, q.y * inv, q.z * inv, q.w * inv};
}

constexpr Vec3 rotate(Quat q, Vec3 v) {
    const Vec3 u{q.x, q.y, q.z};
    const Vec3 t = 2.0f * cross(u, v);
    return v + q.w * t + cross(u, t);
}

// 시뮬레이션 금지: sin/cos 사용
inline Quat quatFromAxisAngle(Vec3 axis, f32 angle) {
    const Vec3 a = normalize(axis);
    const f32 s = std::sin(angle * 0.5f);
    return {a.x * s, a.y * s, a.z * s, std::cos(angle * 0.5f)};
}

inline Quat nlerp(Quat a, Quat b, f32 t) {
    if (a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w < 0.0f) b = {-b.x, -b.y, -b.z, -b.w};
    return normalize({lerp(a.x, b.x, t), lerp(a.y, b.y, t), lerp(a.z, b.z, t), lerp(a.w, b.w, t)});
}

// 결정론적: 월드 공간 각속도로 회전을 적분한다 (q' = q + 0.5 * (w,0) * q * dt). 삼각함수 없음.
inline Quat integrate(Quat q, Vec3 angularVelocity, f32 dt) {
    const Quat dq = Quat{angularVelocity.x, angularVelocity.y, angularVelocity.z, 0.0f} * q;
    const f32 h = 0.5f * dt;
    return normalize({q.x + dq.x * h, q.y + dq.y * h, q.z + dq.z * h, q.w + dq.w * h});
}

struct Mat4 {
    f32 m[4][4] = {};

    static constexpr Mat4 identity() {
        Mat4 r;
        r.m[0][0] = r.m[1][1] = r.m[2][2] = r.m[3][3] = 1.0f;
        return r;
    }
};

// 행 벡터 규약: v * (a * b) == (v * a) * b
constexpr Mat4 operator*(const Mat4& a, const Mat4& b) {
    Mat4 r;
    for (int i = 0; i < 4; ++i)
        for (int j = 0; j < 4; ++j)
            r.m[i][j] = a.m[i][0] * b.m[0][j] + a.m[i][1] * b.m[1][j] + a.m[i][2] * b.m[2][j] + a.m[i][3] * b.m[3][j];
    return r;
}

inline Mat4 lookAtLH(Vec3 eye, Vec3 target, Vec3 up) {
    const Vec3 zAxis = normalize(target - eye);
    const Vec3 xAxis = normalize(cross(up, zAxis));
    const Vec3 yAxis = cross(zAxis, xAxis);
    Mat4 r;
    r.m[0][0] = xAxis.x; r.m[0][1] = yAxis.x; r.m[0][2] = zAxis.x;
    r.m[1][0] = xAxis.y; r.m[1][1] = yAxis.y; r.m[1][2] = zAxis.y;
    r.m[2][0] = xAxis.z; r.m[2][1] = yAxis.z; r.m[2][2] = zAxis.z;
    r.m[3][0] = -dot(xAxis, eye);
    r.m[3][1] = -dot(yAxis, eye);
    r.m[3][2] = -dot(zAxis, eye);
    r.m[3][3] = 1.0f;
    return r;
}

// Reversed-Z 원근 투영: 가까운 면 = 1, 먼 면 = 0. float 깊이 버퍼의 정밀도를 거리 전체에 고르게 쓴다.
inline Mat4 perspectiveReversedZ(f32 fovY, f32 aspect, f32 zNear, f32 zFar) {
    const f32 yScale = 1.0f / std::tan(fovY * 0.5f);
    const f32 xScale = yScale / aspect;
    Mat4 r;
    r.m[0][0] = xScale;
    r.m[1][1] = yScale;
    r.m[2][2] = zNear / (zNear - zFar);
    r.m[2][3] = 1.0f;
    r.m[3][2] = zFar * zNear / (zFar - zNear);
    return r;
}

} // namespace mealkit
