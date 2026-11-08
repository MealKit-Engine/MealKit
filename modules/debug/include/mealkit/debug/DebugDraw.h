#pragma once

#include "mealkit/core/Math.h"
#include "mealkit/core/Types.h"

// Debug drawing for game code: draws hit ranges/paths in editor play and dev build game screens.
//   debug::line(a, b);                      // this frame only
//   debug::box(center, half, rot, kRed, 2); // for 2 seconds
//   debug::text(pos, "hp 30");              // text only when the debug UI (ImGui) is on
// OK to call from sim/: it only queues draw requests and the simulation reads nothing (unrelated to determinism). When rewind/shadow simulation
// reruns the same tick, the same drawings may stack up.
// In shipping builds and headless (builds without MEALKIT_DEBUG_UI) these are empty inline functions, so the code drops out.
namespace mealkit::debug {

inline constexpr u32 kYellow = 0xFF00FFFFu; // sRGB RGBA8 (same layout as render::rgba)
inline constexpr u32 kRed = 0xFF3040FFu;
inline constexpr u32 kGreen = 0xFF40E040u;
inline constexpr u32 kBlue = 0xFFFF9040u;

#if MEALKIT_DEBUG_UI
// seconds = 0 means once, on the next drawn frame
void line(Vec3 a, Vec3 b, u32 color = kYellow, f32 seconds = 0.0f);
void box(Vec3 center, Vec3 half, Quat rotation = {}, u32 color = kYellow, f32 seconds = 0.0f);
void text(Vec3 position, const char* text, u32 color = kYellow, f32 seconds = 0.0f);
#else
inline void line(Vec3, Vec3, u32 = kYellow, f32 = 0.0f) {}
inline void box(Vec3, Vec3, Quat = {}, u32 = kYellow, f32 = 0.0f) {}
inline void text(Vec3, const char*, u32 = kYellow, f32 = 0.0f) {}
#endif

} // namespace mealkit::debug
