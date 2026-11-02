#pragma once

#include "mealkit/core/Reflect.h"
#include "mealkit/sim/Movement.h"
#include "mealkit/sim/Table.h"

// Definition data tables (docs/CODING_MODEL.md "Many items of the same shape"): assets/tables/*.table, editor [Tables] panel.
namespace coop {

// Hero body kinds: chosen per player via match start data (loadout). Players who didn't choose get the first row
MEALKIT_STRUCT(BodyKind) {
    MEALKIT_FIELD(mealkit::f32, speedScale, 1.0f, "이동 속도 배율");
    MEALKIT_FIELD(mealkit::f32, massScale, 1.0f, "질량 배율");
    MEALKIT_FIELD(mealkit::sim::MoveMode, movement, mealkit::sim::MoveMode::Ragdoll, "이동 방식");
};
MEALKIT_TABLE(BodyKind, "tables/bodies.table", 0);

} // namespace coop
