#include "view/Camera.h"

using namespace mealkit;

namespace coop {

void GameCamera::load(const scene::Scene& flat, const CameraTuning& t) {
    brain.load(flat);
    brain.setFallback(t.fallback);
    cutscenes.load();
}

void GameCamera::control(Window& window, const input::Actions& actions, const CameraTuning& t) {
    orbit.control(window, actions, {t.orbitSensitivity, t.zoomStep}); // camrig module: orbit/turn/look/zoom actions
}

void GameCamera::release(Window& window) { orbit.release(window); }

void GameCamera::update(const World& world, const Pawn* me, const Vec3* target, f32 dt, f32 fov, Vec3 shake,
                        std::vector<std::string>& events) {
    camera::BrainInput in = orbit.take(dt, target, fov); // the turn and zoom gathered since last frame
    in.shake = shake;
    const physics::BodyId ignore = me ? me->mover.body : physics::kInvalidBody;
    in.ray = [&world, ignore](Vec3 from, Vec3 to, f32& fraction) { // view-side query, changes nothing
        physics::RayHit hit;
        if (!world.physics.castRay(from, to - from, hit, ignore)) return false;
        fraction = hit.fraction;
        return true;
    };
    brain.update(in, &events);
}

} // namespace coop
