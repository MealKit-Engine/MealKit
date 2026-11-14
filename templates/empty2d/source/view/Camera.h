#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/camera/Brain.h"
#include "mealkit/render/Renderer.h"

// 2D game camera: a fixed orthographic side view looking along +z at the XY plane (CameraTuning center and halfHeight).
// For a camera that follows a character and zooms, add the camrig module (camrig/view/Side2D.h). Unrelated to the simulation.
namespace sandbox {

struct GameCamera {
    void load(const mealkit::scene::Scene& flat, const CameraTuning& tuning);
    void apply(mealkit::render::Viewport viewport, const CameraTuning& tuning, mealkit::render::FrameParams& frame) const;
    mealkit::Vec3 forward() const { return {0.0f, 0.0f, 1.0f}; }
};

} // namespace sandbox
