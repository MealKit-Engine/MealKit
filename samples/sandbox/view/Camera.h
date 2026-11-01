#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/asset/Cutscene.h"
#include "mealkit/camera/Brain.h"
#include "mealkit/input/Actions.h"
#include "mealkit/platform/Window.h"
#include "mealkit/render/Renderer.h"

// Game camera: the engine camera brain (mealkit/camera/Brain.h) over the scene's virtual cameras, with the player's look/zoom input.
// Without virtual cameras in the scene, CameraTuning::fallback (third-person spring arm) follows my Pawn. Unrelated to the simulation.
namespace sandbox {

struct GameCamera {
    mealkit::camera::Brain brain;
    mealkit::asset::Cutscenes cutscenes; // assets/tables/cutscenes.table
    bool orbiting = false;
    mealkit::Vec2 look;
    mealkit::f32 zoom = 0.0f;

    void load(const mealkit::scene::Scene& flat, const CameraTuning& tuning);
    // Every frame: actions orbit (rotate with look while held), turn (rotate immediately, pad), zoom (zoom amount)
    void control(mealkit::Window& window, const mealkit::input::Actions& actions, const CameraTuning& tuning);
    void release(mealkit::Window& window); // menus open: no turning
    // Moves the brain. target = my Pawn (nullptr while waiting to respawn), world for spring arm wall rays
    void update(const World& world, const Pawn* me, const mealkit::Vec3* target, mealkit::f32 dt, mealkit::f32 fov,
                mealkit::Vec3 shake, std::vector<std::string>& events);
    void apply(mealkit::render::Viewport viewport, const CameraTuning& tuning, mealkit::render::FrameParams& frame) const {
        brain.apply(viewport, tuning.nearPlane, tuning.farPlane, frame);
    }
    mealkit::Vec3 forward() const { return brain.flatForward(); } // horizontal forward (move input → world direction)
};

} // namespace sandbox
