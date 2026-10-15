#include "view/Draw.h"

#include <cmath>
#include <iterator>

using namespace mealkit;

namespace sandbox {
namespace {

constexpr u32 kPlayerColors[net::kMaxPlayers] = {
    render::rgba(66, 135, 245), render::rgba(235, 64, 52),  render::rgba(52, 199, 89),  render::rgba(250, 204, 21),
    render::rgba(168, 85, 247), render::rgba(236, 72, 153), render::rgba(20, 184, 166), render::rgba(249, 115, 22),
};

constexpr u32 kPropColors[] = {
    render::rgba(232, 98, 82),   render::rgba(245, 176, 65),  render::rgba(92, 172, 238),
    render::rgba(120, 200, 120), render::rgba(190, 120, 220), render::rgba(236, 236, 230),
};
static_assert(std::size(kPropColors) == kPropColorCount);

} // namespace

void applyEnvironment(const scene::Environment& env, render::FrameParams& frame) {
    frame.sunDirection = env.sunDirection;
    frame.sunColor = env.sunColor;
    frame.skyColor = env.skyColor;
    frame.groundColor = env.groundColor;
    frame.fogColor = env.fogColor;
    frame.fogDensity = env.fogDensity;
}

void drawScene(const scene::Scene& scene, asset::ModelCache& models, const Vec3& eye, render::DrawList& out) {
    const render::MeshId box = models.boxMesh();
    for (const scene::Entity& e : scene.entities) {
        if (e.is(scene::kTypeSpawn)) continue; // 게임에서는 보이지 않음
        const asset::Model* model = e.model.empty() ? nullptr : models.get(e.model);
        const render::MeshId mesh = model ? models.pickLod(*model, eye, e.position, e.scale) : box;
        out.add(mesh, {e.position, e.color, e.rotation, e.scale, e.flags});
    }
}

void drawWorld(const WorldState& previous, const WorldState& current, f32 alpha, u8 localPlayer, f64 now,
               render::MeshId box, WorldView& view, render::DrawList& out) {
    for (auto [id, b] : current.props) {
        const Prop* before = previous.props.get(id); // 이번 틱에 생긴 상자는 직전 상태가 없다
        const Prop& a = before ? *before : b;
        out.add(box, {lerp(a.position, b.position, alpha), kPropColors[b.colorIndex % kPropColorCount],
                      nlerp(a.rotation, b.rotation, alpha), b.halfExtents * 2.0f, 0});
    }
    view.facing.resize(current.pawns.capacity(), 0.0f);
    for (auto [id, b] : current.pawns) {
        const Pawn* before = previous.pawns.get(id); // 이번 틱에 생긴 Pawn은 직전 상태가 없다
        const Pawn& a = before ? *before : b;
        const Vec3 pos = lerp(a.position, b.position, alpha);
        f32& yaw = view.facing[id.slot];
        if (!before) yaw = 0.0f;
        if (b.velocity.x * b.velocity.x + b.velocity.z * b.velocity.z > 0.25f)
            yaw = std::atan2(b.velocity.x, b.velocity.z);
        const Quat facing = quatFromAxisAngle({0, 1, 0}, yaw);
        const u8 player = pawnPlayer(current, b);
        const u32 color = player != kNoPlayer ? kPlayerColors[player % net::kMaxPlayers] : render::rgba(150, 150, 160);
        out.add(box, {pos, color, facing, {0.8f, kPlayerHalfHeight * 2, 0.8f}, 0});
        out.add(
            box,
            {pos + rotate(facing, Vec3{0, 0.45f, 0.4f}), render::rgba(30, 30, 36), facing, {0.5f, 0.18f, 0.12f}, 0});
        if (player == localPlayer) { // 내 캐릭터 표시
            const Quat spin = quatFromAxisAngle({0, 1, 0}, static_cast<f32>(now) * 3.0f);
            out.add(box, {pos + Vec3{0, 1.6f, 0}, render::rgba(255, 230, 80), spin, {0.3f, 0.3f, 0.3f}, 0});
        }
    }
}

} // namespace sandbox
