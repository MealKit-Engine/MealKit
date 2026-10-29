#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/render/DrawList.h"

#include <vector>

// View effects caused by tick events (world.events). For now push/jump pad shockwaves (sound/particles go the same route when added).
// Time is the view clock (GameSession counts it with frame dt), unrelated to the simulation.
namespace sandbox {

struct Effects {
    struct Shockwave {
        Vec3 position;
        mealkit::f64 start = 0.0;
        f32 radius = 0.0f;
    };
    std::vector<Shockwave> shockwaves;

    // pawnHalfHeight: from the character event position (body center) to underfoot (world.tuning)
    void onEvent(const GameEvent& event, mealkit::f64 now, const ShockwaveLook& look, f32 pawnHalfHeight);
    // Removes finished effects (every frame, so they don't pile up even on frames that aren't drawn)
    void prune(mealkit::f64 now, const ShockwaveLook& look);
    void draw(mealkit::f64 now, const ShockwaveLook& look, mealkit::render::MeshId box,
              mealkit::render::DrawList& out) const;
};

} // namespace sandbox
