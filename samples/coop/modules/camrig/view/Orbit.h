#pragma once

#include "mealkit/camera/Brain.h"
#include "mealkit/input/Actions.h"
#include "mealkit/platform/Window.h"

// Orbit control (docs/index.md): the player turns and zooms the engine's camera::Brain (third person spring arm, scene
// virtual cameras) with input actions. Hold "orbit" and move "look" (mouse, pixels) to turn, "turn" turns at once (gamepad stick),
// "zoom" changes the distance. View only: nothing here enters the simulation.
namespace camrig {

struct OrbitSettings {
    mealkit::f32 sensitivity = 0.005f; // radians per look unit (mouse pixel, stick rate)
    mealkit::f32 zoomStep = 0.5f;      // distance per zoom step (m)
};

class OrbitControl {
public:
    // Every frame while the camera may move (not under menus)
    void control(mealkit::Window& window, const mealkit::input::Actions& actions, const OrbitSettings& s) {
        if (actions.pressed("orbit")) m_orbiting = true;
        if (!actions.down("orbit") || !window.focused()) m_orbiting = false;
        window.setCursorCaptured(m_orbiting);
        mealkit::Vec2 l = actions.axis2("turn"); // gamepad stick: immediately, without holding
        if (m_orbiting) l += actions.axis2("look"); // the mouse is in pixels, up is +
        m_look += mealkit::Vec2{l.x * s.sensitivity, -l.y * s.sensitivity};
        m_zoom -= actions.axis("zoom") * s.zoomStep;
    }
    // Menus opened: stop turning and give the cursor back
    void release(mealkit::Window& window) {
        window.setCursorCaptured(false);
        m_orbiting = false;
    }
    // The brain input for this frame with the turn and zoom gathered since the last call (then cleared).
    // target: what the camera follows (nullptr keeps the last one). Set ray/shake/findTarget on the result as needed
    mealkit::camera::BrainInput take(mealkit::f32 dt, const mealkit::Vec3* target, mealkit::f32 fov) {
        mealkit::camera::BrainInput in;
        in.dt = dt;
        in.look = m_look;
        in.zoom = m_zoom;
        in.hasTarget = target != nullptr;
        if (target) in.target = *target;
        in.defaultFov = fov;
        m_look = {};
        m_zoom = 0.0f;
        return in;
    }
    bool orbiting() const { return m_orbiting; }

private:
    bool m_orbiting = false;
    mealkit::Vec2 m_look;
    mealkit::f32 m_zoom = 0.0f;
};

} // namespace camrig
