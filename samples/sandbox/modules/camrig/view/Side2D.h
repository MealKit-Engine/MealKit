#pragma once

#include "mealkit/core/Math.h"
#include "mealkit/input/Actions.h"
#include "mealkit/render/Renderer.h"

#include <algorithm>
#include <cmath>

// 2D side camera (docs/index.md): orthographic, looking along +z at the XY plane (mealkit.project view2d), following a point
// with frame-rate independent smoothing. The "zoom" action changes the visible height. View only.
namespace camrig {

struct Side2DSettings {
    mealkit::f32 halfHeight = 7.0f;      // half the visible height (m)
    mealkit::f32 minHalfHeight = 3.0f;   // closest zoom (m)
    mealkit::f32 maxHalfHeight = 20.0f;  // farthest zoom (m)
    mealkit::f32 zoomStep = 1.0f;        // per zoom step (m)
    mealkit::f32 followHeight = 2.0f;    // look this far above the target (m)
    mealkit::f32 followSharpness = 6.0f; // how fast it catches up (1/s; 0 = jump)
    mealkit::f32 nearPlane = -100.0f;    // near clip (m, may be negative: orthographic)
    mealkit::f32 farPlane = 200.0f;      // far clip (m)
};

class Side2DCamera {
public:
    void reset() { m_placed = false, m_halfHeight = 0.0f; }
    void control(const mealkit::input::Actions& actions, const Side2DSettings& s) { m_zoom -= actions.axis("zoom") * s.zoomStep; }
    // target: the followed point (nullptr = stay)
    void update(const mealkit::Vec3* target, mealkit::f32 dt, const Side2DSettings& s) {
        m_halfHeight = std::clamp((m_halfHeight > 0.0f ? m_halfHeight : s.halfHeight) + m_zoom, s.minHalfHeight, s.maxHalfHeight);
        m_zoom = 0.0f;
        if (!target) return;
        const mealkit::Vec3 goal{target->x, target->y + s.followHeight, 0.0f};
        if (!m_placed || s.followSharpness <= 0.0f) {
            m_position = goal;
            m_placed = true;
            return;
        }
        m_position = mealkit::lerp(m_position, goal, 1.0f - std::exp(-s.followSharpness * dt));
    }
    void apply(mealkit::render::Viewport viewport, const Side2DSettings& s, mealkit::render::FrameParams& frame) const {
        const mealkit::f32 width = viewport.width > 0.0f ? viewport.width : 1.0f;
        const mealkit::f32 height = viewport.height > 0.0f ? viewport.height : 1.0f;
        const mealkit::Vec3 eye{m_position.x, m_position.y, -50.0f};
        frame.viewport = viewport;
        frame.viewProj = mealkit::lookAtLH(eye, eye + mealkit::Vec3{0, 0, 1}, {0, 1, 0}) *
                         mealkit::orthographicReversedZ(m_halfHeight > 0.0f ? m_halfHeight : s.halfHeight, width / height,
                                                        s.nearPlane, s.farPlane);
        frame.cameraPos = eye;
        frame.cameraForward = {0, 0, 1};
    }
    mealkit::Vec3 position() const { return m_position; }
    mealkit::Vec3 forward() const { return {0.0f, 0.0f, 1.0f}; }

private:
    mealkit::Vec3 m_position;
    mealkit::f32 m_halfHeight = 0.0f;
    mealkit::f32 m_zoom = 0.0f;
    bool m_placed = false;
};

} // namespace camrig
