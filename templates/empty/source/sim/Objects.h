#pragma once

#include "mealkit/core/Reflect.h"

// Scene object kinds declared by the game. Shown in the editor [Add] menu and inspector, and field values are saved in the scene file.
// The World reads them with scene::readObject<T>(entity) when a match starts (sim/World.cpp loadScene).
// Values that differ per map (ranges, counts, heights) go into the position/size/fields of these objects instead of code constants.
//
// Example (a box whose top launches characters upward):
//   MEALKIT_OBJECT(JumpPad) {
//       static constexpr const char* kObjectLabel = "Jump pad";
//       MEALKIT_FIELD(mealkit::f32, power, 14.0f, "Jump power (m/s)");
//   };
// Areas (no drawing, only position/size) add: static constexpr mealkit::u32 kObjectFlags = mealkit::reflect::kObjectArea;
namespace sandbox {} // namespace sandbox
