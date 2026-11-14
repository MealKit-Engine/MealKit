#include "view/Camera.h"

using namespace mealkit;

namespace sandbox {

void GameCamera::load(const scene::Scene& flat, const CameraTuning& t) {
    brain.load(flat);
    brain.setFallback(t.fallback);
}

void GameCamera::update(const World& world, const Pawn* me, const Vec3* target, f32 dt, f32 fov) {
    camera::BrainInput in;
    in.dt = dt;
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
}

} // namespace sandbox
