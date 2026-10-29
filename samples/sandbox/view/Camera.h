#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/input/Actions.h"
#include "mealkit/platform/Window.h"
#include "mealkit/render/Renderer.h"

// Third-person orbit camera: rotate with right drag, zoom with the wheel. Unrelated to the simulation, so it moves immediately without input delay.
namespace sandbox {

// Values (field of view, sensitivity, distance/angle ranges) are CameraTuning in view tuning (view/ViewTuning.h).
struct OrbitCamera {
    f32 yaw = 0.0f;
    f32 pitch = 0.0f;
    f32 distance = 0.0f;
    bool orbiting = false;
    bool placed = false; // placed at the tuning's start values on the first control()

    // Every frame: actions orbit (rotate with look while held), turn (rotate immediately, pad), zoom (zoom amount)
    void control(mealkit::Window& window, const mealkit::input::Actions& actions, const CameraTuning& tuning);
    Vec3 forward() const; // horizontal forward direction (for turning move input into a world direction)
    Vec3 eye(const Vec3& focus) const;
    // Fills the viewport, view/projection matrices, and camera position
    void apply(const Vec3& focus, mealkit::render::Viewport viewport, const CameraTuning& tuning,
               mealkit::render::FrameParams& frame) const;
};

} // namespace sandbox
