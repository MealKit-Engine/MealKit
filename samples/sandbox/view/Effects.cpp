#include "view/Effects.h"

#include <algorithm>
#include <cmath>

using namespace mealkit;

namespace sandbox {
namespace {

constexpr f32 kTwoPi = 6.28318531f;

} // namespace

void Effects::onEvent(const GameEvent& event, f64 now, const ShockwaveLook& look, f32 pawnHalfHeight) {
    const Vec3 feet = event.position - Vec3{0.0f, pawnHalfHeight - look.groundOffset, 0.0f};
    switch (event.kind) {
    case EventKind::Push: shockwaves.push_back({feet, now, event.value}); break; // push range
    case EventKind::Bounce:
        shockwaves.push_back({event.position + Vec3{0.0f, look.groundOffset, 0.0f}, now, look.bounceRadius});
        break;
    case EventKind::Spawn: shockwaves.push_back({feet, now, look.spawnRadius}); break;
    case EventKind::Fall:
    case EventKind::Jump:
    case EventKind::Impact:
    case EventKind::Countdown:
    case EventKind::MatchStart:
    case EventKind::MatchEnd: break;
    }
}

void Effects::prune(f64 now, const ShockwaveLook& look) {
    const f64 seconds = look.seconds;
    std::erase_if(shockwaves, [&](const Shockwave& w) { return now - w.start >= seconds; });
}

// A ring spreading from underfoot to the push range (drawn with thin box segments)
void Effects::draw(f64 now, const ShockwaveLook& look, render::MeshId box, render::DrawList& out) const {
    const u32 segments = std::max(look.segments, 3u);
    const f64 seconds = std::max(look.seconds, 0.01f);
    for (const Shockwave& w : shockwaves) {
        const f32 t = std::clamp(static_cast<f32>((now - w.start) / seconds), 0.0f, 1.0f);
        const f32 radius = w.radius * t;
        const f32 segment = kTwoPi * std::max(radius, 0.1f) / static_cast<f32>(segments);
        const f32 thickness = look.startThickness + (look.endThickness - look.startThickness) * t;
        for (u32 i = 0; i < segments; ++i) {
            const f32 angle = kTwoPi * static_cast<f32>(i) / static_cast<f32>(segments);
            const Vec3 offset{std::sin(angle) * radius, 0.0f, std::cos(angle) * radius};
            out.add(box, {w.position + offset, look.color.value, quatFromAxisAngle({0, 1, 0}, angle),
                          {segment, thickness, thickness}, 0});
        }
    }
}

} // namespace sandbox
