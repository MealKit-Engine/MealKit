#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/sim/Table.h"

// Definition data tables (docs/CODING_MODEL.md "Many items of the same shape"): game data with many rows of the same shape.
// Values are in assets/tables/*.table (editor [Tables] panel, CSV import/export). When a match starts, app/Headless.cpp reads them and copies them into the World.
// Other data (scene objects etc.) points to rows with RowRef<row type> (name id).
namespace sandbox {

// Box kinds: chosen by box spawn areas (PropSpawnArea.kind)
MEALKIT_STRUCT(PropKind) {
    MEALKIT_FIELD(mealkit::f32, sizeMin, 0.28f, "반 크기 최소 (m)");
    MEALKIT_FIELD(mealkit::f32, sizeMax, 0.64f, "반 크기 최대 (m)");
    MEALKIT_FIELD(mealkit::f32, stretchMin, 0.8f, "축별 늘림 최소 (배)");
    MEALKIT_FIELD(mealkit::f32, stretchMax, 1.2f, "축별 늘림 최대 (배)");
    MEALKIT_FIELD(mealkit::f32, density, 200.0f, "밀도 (kg/m³)");
    MEALKIT_FIELD(mealkit::f32, minMass, 5.0f, "최소 질량 (kg)");
    MEALKIT_FIELD(mealkit::f32, friction, 0.6f, "마찰");
    MEALKIT_FIELD(mealkit::f32, restitution, 0.15f, "반발");
};
MEALKIT_TABLE(PropKind, "tables/props.table", 0); // simulation table: goes into the content hash

// Character body kinds: chosen per player via match start data (loadout). AI and players who didn't choose get the first row
MEALKIT_STRUCT(BodyKind) {
    MEALKIT_FIELD(mealkit::f32, speedScale, 1.0f, "이동 속도 배율");
    MEALKIT_FIELD(mealkit::f32, massScale, 1.0f, "질량 배율");
};
MEALKIT_TABLE(BodyKind, "tables/bodies.table", 0);

} // namespace sandbox
