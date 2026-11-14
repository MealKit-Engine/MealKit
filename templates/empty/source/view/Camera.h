#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/camera/Brain.h"
#include "mealkit/render/Renderer.h"

// Game camera: the engine camera brain (mealkit/camera/Brain.h) over the scene's virtual cameras. Without virtual cameras in the
// scene, CameraTuning::fallback (third-person spring arm) follows my character. The player doesn't turn it: for mouse/stick
// orbit and zoom, add the camrig module (camrig/view/Orbit.h). Unrelated to the simulation.
namespace sandbox {

struct GameCamera {
    mealkit::camera::Brain brain;

    void load(const mealkit::scene::Scene& flat, const CameraTuning& tuning);
    // Moves the brain. target = my character (nullptr while waiting to respawn), world for spring arm wall rays
    void update(const World& world, const Pawn* me, const mealkit::Vec3* target, mealkit::f32 dt, mealkit::f32 fov);
    void apply(mealkit::render::Viewport viewport, const CameraTuning& tuning, mealkit::render::FrameParams& frame) const {
        brain.apply(viewport, tuning.nearPlane, tuning.farPlane, frame);
    }
    mealkit::Vec3 forward() const { return brain.flatForward(); } // horizontal forward (move input → world direction)
};

} // namespace sandbox
