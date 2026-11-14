#include "view/Camera.h"

using namespace mealkit;

namespace sandbox {

void GameCamera::load(const scene::Scene& flat, const CameraTuning& t) {
    (void)flat;
    (void)t;
}

void GameCamera::apply(render::Viewport viewport, const CameraTuning& t, render::FrameParams& frame) const {
    const f32 width = viewport.width > 0.0f ? viewport.width : 1.0f;
    const f32 height = viewport.height > 0.0f ? viewport.height : 1.0f;
    const Vec3 eye{t.center.x, t.center.y, -50.0f};
    frame.viewport = viewport;
    frame.viewProj = lookAtLH(eye, eye + Vec3{0, 0, 1}, {0, 1, 0}) * orthographicReversedZ(t.halfHeight, width / height, t.nearPlane, t.farPlane);
    frame.cameraPos = eye;
    frame.cameraForward = {0, 0, 1};
}

} // namespace sandbox
