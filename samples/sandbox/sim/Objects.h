#pragma once

#include "mealkit/core/Reflect.h"
#include "sim/Tables.h"

// Scene object kinds declared by the game. Shown in the editor [Add] menu and inspector, and field values are saved in the scene file.
// The World reads them with scene::readObject<T>(entity) when a match starts (sim/World.cpp loadScene).
// Values that differ per map (ranges, counts, heights) go into the position/size/fields of these objects instead of code constants (docs/CODING_MODEL.md "Where to put numbers").
namespace sandbox {

// A pad that launches you upward when stepped on. The top face of the box placed in the scene is where you step (rotation ignored).
MEALKIT_OBJECT(JumpPad) {
    static constexpr const char* kObjectLabel = "점프대";
    MEALKIT_FIELD(mealkit::f32, power, 14.0f, "점프 힘 (m/s)");
    MEALKIT_FIELD(mealkit::Ticks, cooldown, mealkit::Ticks::seconds(0.6667f), "재사용 대기 (초)");
};

// Match area (one per scene). The box's size is the rule itself (rotation ignored):
//   width/depth = range the AI roams, range where fallen boxes are dropped again
//   bottom = falling below this is death / top = height from which fallen boxes are dropped again
MEALKIT_OBJECT(MatchArea) {
    static constexpr const char* kObjectLabel = "경기 영역";
    static constexpr mealkit::u32 kObjectFlags = mealkit::reflect::kObjectArea;
};

// When a match starts, boxes are scattered inside this area (in scene order if there are several). The kind is a row of the box kind table (tables/props.table)
MEALKIT_OBJECT(PropSpawnArea) {
    static constexpr const char* kObjectLabel = "상자 생성 영역";
    static constexpr mealkit::u32 kObjectFlags = mealkit::reflect::kObjectArea;
    MEALKIT_FIELD(mealkit::u32, count, 2000, "상자 수");
    MEALKIT_FIELD(mealkit::RowRef<PropKind>, kind, "crate", "상자 종류");
};

} // namespace sandbox
