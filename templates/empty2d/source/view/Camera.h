#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/camera/Brain.h"
#include "mealkit/input/Actions.h"
#include "mealkit/platform/Window.h"
#include "mealkit/render/Renderer.h"

// 2D game camera: orthographic side view looking along +z at the XY plane, following my character (CameraTuning).
// The wheel zooms (visible height). Unrelated to the simulation.
namespace sandbox {

struct GameCamera {
    mealkit::Vec3 position;     // looked-at point on the plane
    mealkit::f32 halfHeight = 0.0f; // 0 until load
    mealkit::f32 zoom = 0.0f;
    bool placed = false;

    void load(const mealkit::scene::Scene& flat, const CameraTuning& tuning);
    void control(mealkit::Window& window, const mealkit::input::Actions& actions, const CameraTuning& tuning);
    void release(mealkit::Window& window);
    void update(const World& world, const Pawn* me, const mealkit::Vec3* target, mealkit::f32 dt, mealkit::f32 fov);
    void apply(mealkit::render::Viewport viewport, const CameraTuning& tuning, mealkit::render::FrameParams& frame) const;
    mealkit::Vec3 forward() const { return {0.0f, 0.0f, 1.0f}; }

private:
    CameraTuning m_tuning;
};

} // namespace sandbox
