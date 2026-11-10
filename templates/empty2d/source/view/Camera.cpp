#include "view/Camera.h"

#include <algorithm>
#include <cmath>

using namespace mealkit;

namespace sandbox {

void GameCamera::load(const scene::Scene& flat, const CameraTuning& t) {
    (void)flat;
    m_tuning = t;
    halfHeight = t.halfHeight;
    placed = false;
}

void GameCamera::control(Window& window, const input::Actions& actions, const CameraTuning& t) {
    (void)window;
    m_tuning = t; // view.tuning is reread on change
    zoom -= actions.axis("zoom") * t.zoomStep;
}

void GameCamera::release(Window& window) { window.setCursorCaptured(false); }

void GameCamera::update(const World& world, const Pawn* me, const Vec3* target, f32 dt, f32 fov) {
    (void)world;
    (void)me;
    (void)fov;
    const CameraTuning& t = m_tuning;
    halfHeight = std::clamp((halfHeight > 0.0f ? halfHeight : t.halfHeight) + zoom, t.minHalfHeight, t.maxHalfHeight);
    zoom = 0.0f;
    if (!target) return; // waiting to respawn: stay
    const Vec3 goal{target->x, target->y + t.followHeight, 0.0f};
    if (!placed) {
        position = goal;
        placed = true;
        return;
    }
    const f32 k = 1.0f - std::exp(-t.followSharpness * dt); // frame-rate independent smoothing
    position = lerp(position, goal, k);
}

void GameCamera::apply(render::Viewport viewport, const CameraTuning& t, render::FrameParams& frame) const {
    const f32 width = viewport.width > 0.0f ? viewport.width : 1.0f;
    const f32 height = viewport.height > 0.0f ? viewport.height : 1.0f;
    const Vec3 eye{position.x, position.y, -50.0f};
    frame.viewport = viewport;
    frame.viewProj = lookAtLH(eye, eye + Vec3{0, 0, 1}, {0, 1, 0}) *
                     orthographicReversedZ(halfHeight > 0.0f ? halfHeight : t.halfHeight, width / height, t.nearPlane, t.farPlane);
    frame.cameraPos = eye;
    frame.cameraForward = {0, 0, 1};
}

} // namespace sandbox
