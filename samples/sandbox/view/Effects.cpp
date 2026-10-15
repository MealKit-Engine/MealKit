#include "view/Effects.h"

#include <algorithm>
#include <cmath>

using namespace mealkit;

namespace sandbox {
namespace {

constexpr f64 kShockwaveSeconds = 0.35;
constexpr u32 kShockwaveSegments = 16;

} // namespace

void Effects::onEvent(const GameEvent& event, f64 now) {
    switch (event.kind) {
    case EventKind::Push: shockwaves.push_back({event.position - Vec3{0.0f, kPlayerHalfHeight - 0.05f, 0.0f}, now, event.radius}); break;
    case EventKind::Bounce: shockwaves.push_back({event.position + Vec3{0.0f, 0.05f, 0.0f}, now, event.radius}); break;
    }
}

void Effects::prune(f64 now) {
    std::erase_if(shockwaves, [now](const Shockwave& w) { return now - w.start >= kShockwaveSeconds; });
}

// 발밑에서 밀치기 범위까지 퍼지는 고리 (얇은 상자 조각으로 그린다)
void Effects::draw(f64 now, render::MeshId box, render::DrawList& out) const {
    for (const Shockwave& w : shockwaves) {
        const f32 t = std::clamp(static_cast<f32>((now - w.start) / kShockwaveSeconds), 0.0f, 1.0f);
        const f32 radius = w.radius * t;
        const f32 segment = 2.0f * 3.14159265f * std::max(radius, 0.1f) / kShockwaveSegments;
        const f32 thickness = 0.25f * (1.0f - t) + 0.05f;
        const Vec3 center = w.position;
        for (u32 i = 0; i < kShockwaveSegments; ++i) {
            const f32 angle = 2.0f * 3.14159265f * static_cast<f32>(i) / kShockwaveSegments;
            const Vec3 offset{std::sin(angle) * radius, 0.0f, std::cos(angle) * radius};
            out.add(box, {center + offset, render::rgba(255, 240, 200), quatFromAxisAngle({0, 1, 0}, angle),
                          {segment, thickness, thickness}, 0});
        }
    }
}

} // namespace sandbox
