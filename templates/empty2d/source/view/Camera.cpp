#include "view/Camera.h"

#include "mealkit/render/Sprites.h"

using namespace mealkit;

namespace sandbox {

void GameCamera::load(const scene::Scene& flat, const CameraTuning& t) {
    (void)flat;
    (void)t;
}

void GameCamera::apply(render::Viewport viewport, const CameraTuning& t, render::FrameParams& frame) const {
    const f32 width = viewport.width > 0.0f ? viewport.width : 1.0f;
    const f32 height = viewport.height > 0.0f ? viewport.height : 1.0f;
    // pixelsPerUnit > 0: whole-number zoom and a center on the screen-pixel grid (render::snapPixelView), for pixel art
    const render::PixelView view = render::snapPixelView({t.center.x, t.center.y}, t.halfHeight, height, t.pixelsPerUnit);
    const Vec3 eye{view.center.x, view.center.y, -50.0f};
    frame.viewport = viewport;
    frame.viewProj = lookAtLH(eye, eye + Vec3{0, 0, 1}, {0, 1, 0}) * orthographicReversedZ(view.halfHeight, width / height, t.nearPlane, t.farPlane);
    frame.cameraPos = eye;
    frame.cameraForward = {0, 0, 1};
}

} // namespace sandbox
