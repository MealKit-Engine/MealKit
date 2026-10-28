#pragma once

#include "mealkit/core/Math.h"
#include "mealkit/core/Types.h"

// 게임 코드의 디버그 그리기: 판정 범위·경로를 에디터 플레이와 개발 빌드 게임 화면에 그린다.
//   debug::line(a, b);                      // 이번 프레임만
//   debug::box(center, half, rot, kRed, 2); // 2초 동안
//   debug::text(pos, "hp 30");              // 글자는 디버그 UI(ImGui)가 켜져 있을 때만
// sim/에서 불러도 된다: 그리기 요청만 쌓고 시뮬레이션이 읽는 것은 없다(결정론과 무관). 되감기·그림자 시뮬레이션이
// 같은 틱을 다시 돌리면 같은 그림이 겹쳐 쌓일 수 있다.
// 출시 빌드와 헤드리스(MEALKIT_DEBUG_UI가 없는 빌드)에서는 빈 inline 함수라 코드가 빠진다.
namespace mealkit::debug {

inline constexpr u32 kYellow = 0xFF00FFFFu; // sRGB RGBA8 (render::rgba와 같은 배치)
inline constexpr u32 kRed = 0xFF3040FFu;
inline constexpr u32 kGreen = 0xFF40E040u;
inline constexpr u32 kBlue = 0xFFFF9040u;

#if MEALKIT_DEBUG_UI
// seconds = 0이면 다음에 그리는 프레임 한 번
void line(Vec3 a, Vec3 b, u32 color = kYellow, f32 seconds = 0.0f);
void box(Vec3 center, Vec3 half, Quat rotation = {}, u32 color = kYellow, f32 seconds = 0.0f);
void text(Vec3 position, const char* text, u32 color = kYellow, f32 seconds = 0.0f);
#else
inline void line(Vec3, Vec3, u32 = kYellow, f32 = 0.0f) {}
inline void box(Vec3, Vec3, Quat = {}, u32 = kYellow, f32 = 0.0f) {}
inline void text(Vec3, const char*, u32 = kYellow, f32 = 0.0f) {}
#endif

} // namespace mealkit::debug
