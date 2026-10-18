#include "view/Draw.h"

#include <cmath>

using namespace mealkit;

namespace sandbox {

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
               const ViewTuning& look, render::MeshId box, WorldView& view, render::DrawList& out) {
    const PawnLook& pl = look.pawn;
    for (auto [id, b] : current.props) {
        const Prop* before = previous.props.get(id); // 이번 틱에 생긴 상자는 직전 상태가 없다
        const Prop& a = before ? *before : b;
        out.add(box, {lerp(a.position, b.position, alpha), look.propColors[b.colorIndex % kPropColorCount].value,
                      nlerp(a.rotation, b.rotation, alpha), b.halfExtents * 2.0f, 0});
    }
    view.facing.resize(current.pawns.capacity(), 0.0f);
    for (auto [id, b] : current.pawns) {
        const Pawn* before = previous.pawns.get(id); // 이번 틱에 생긴 Pawn은 직전 상태가 없다
        const Pawn& a = before ? *before : b;
        const Vec3 pos = lerp(a.position, b.position, alpha);
        f32& yaw = view.facing[id.slot];
        if (!before) yaw = 0.0f;
        if (b.velocity.x * b.velocity.x + b.velocity.z * b.velocity.z > pl.turnMinSpeed * pl.turnMinSpeed)
            yaw = std::atan2(b.velocity.x, b.velocity.z);
        const Quat facing = quatFromAxisAngle({0, 1, 0}, yaw);
        const u8 player = pawnPlayer(current, b);
        const Color color = player != kNoPlayer ? look.playerColors[player % net::kMaxPlayers] : pl.aiColor;
        out.add(box, {pos, color.value, facing, pl.bodySize, 0});
        out.add(box, {pos + rotate(facing, pl.visorOffset), pl.visorColor.value, facing, pl.visorSize, 0});
        if (player == localPlayer) { // 내 캐릭터 표시
            const Quat spin = quatFromAxisAngle({0, 1, 0}, static_cast<f32>(now) * pl.markerSpin);
            const f32 size = pl.markerSize;
            out.add(box, {pos + Vec3{0, pl.markerHeight, 0}, pl.markerColor.value, spin, {size, size, size}, 0});
        }
    }
}

} // namespace sandbox
