#include "view/Camera.h"

using namespace mealkit;

namespace sandbox {

void GameCamera::load(const scene::Scene& flat, const CameraTuning& t) {
    brain.load(flat);
    brain.setFallback(t.fallback);
    orbiting = false;
}

void GameCamera::control(Window& window, const input::Actions& actions, const CameraTuning& t) {
    if (actions.pressed("orbit")) orbiting = true;
    if (!actions.down("orbit") || !window.focused()) orbiting = false;
    window.setCursorCaptured(orbiting);
    Vec2 l = actions.axis2("turn"); // gamepad stick: immediately, without holding
    if (orbiting) l += actions.axis2("look"); // the mouse is in pixels, up is +
    look += Vec2{l.x * t.orbitSensitivity, -l.y * t.orbitSensitivity};
    zoom -= actions.axis("zoom") * t.zoomStep;
}

void GameCamera::release(Window& window) {
    window.setCursorCaptured(false);
    orbiting = false;
}

void GameCamera::update(const World& world, const Pawn* me, const Vec3* target, f32 dt, f32 fov) {
    camera::BrainInput in;
    in.dt = dt;
    in.look = look;
    in.zoom = zoom;
    in.hasTarget = target != nullptr;
    if (target) in.target = *target;
    in.defaultFov = fov;
    const physics::BodyId ignore = me ? me->mover.body : physics::kInvalidBody;
    in.ray = [&world, ignore](Vec3 from, Vec3 to, f32& fraction) { // view-side query, changes nothing
        physics::RayHit hit;
        if (!world.physics.castRay(from, to - from, hit, ignore)) return false;
        fraction = hit.fraction;
        return true;
    };
    brain.update(in);
    look = {};
    zoom = 0.0f;
}

} // namespace sandbox
