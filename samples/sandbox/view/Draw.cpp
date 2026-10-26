#include "view/Draw.h"

#include <cmath>

using namespace mealkit;

namespace sandbox {

void applyEnvironment(const scene::Environment& env, asset::ModelCache& models, render::FrameParams& frame) {
    frame.sunDirection = env.sunDirection;
    frame.sunColor = env.sunColor;
    frame.skyColor = env.skyColor;
    frame.groundColor = env.groundColor;
    frame.fogColor = env.fogColor;
    frame.fogDensity = env.fogDensity;
    frame.environment = models.environment(env.hdri);
    frame.environmentIntensity = env.hdriIntensity;
    frame.environmentRotation = env.hdriRotation * (3.14159265f / 180.0f);
}

void applyProbes(const scene::Scene& scene, asset::ModelCache& models, std::vector<render::ReflectionProbe>& probes,
                 render::FrameParams& frame) {
    models.probes(scene, probes);
    frame.probes = probes.data();
    frame.probeCount = static_cast<u32>(probes.size());
}

void drawScene(const scene::Scene& scene, asset::ModelCache& models, const Vec3& eye, render::DrawList& out) {
    const render::MeshId box = models.boxMesh();
    for (const scene::Entity& e : scene.entities) {
        if (!scene::visibleInGame(e, &reflect::moduleObjectTypes())) continue; // 스폰 지점·영역은 게임에서 보이지 않음
        const asset::Model* model = e.model.empty() ? nullptr : models.get(e.model);
        const render::MeshId mesh = model ? models.pickLod(*model, eye, e.position, e.scale) : box;
        out.add(mesh, {e.position, e.color, e.rotation, e.scale, e.flags});
    }
}

void onWorldEvent(const GameEvent& e, const WorldState& state, f64 viewTime, WorldView& view) {
    if (e.kind != EventKind::Push) return;
    view.hitTime.resize(state.pawns.capacity(), -1.0);
    for (auto [id, pawn] : state.pawns) {
        const Vec3 d = pawn.position - e.position;
        const f32 dist2 = d.x * d.x + d.y * d.y + d.z * d.z;
        if (dist2 > 1e-4f && dist2 <= e.value * e.value) view.hitTime[id.slot] = viewTime; // 민 Pawn(거리 0)은 빼고
    }
}

void drawWorld(const WorldState& previous, const WorldState& current, f32 alpha, u8 localPlayer, f64 now, f64 viewTime,
               const ViewTuning& look, render::MeshId box, WorldView& view, render::DrawList& out, const Vec3& localOffset) {
    const PawnLook& pl = look.pawn;
    view.hitTime.resize(current.pawns.capacity(), -1.0);
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
        const u8 player = pawnPlayer(current, b);
        const Vec3 pos = lerp(a.position, b.position, alpha) + (player == localPlayer ? localOffset : Vec3{});
        f32& yaw = view.facing[id.slot];
        if (!before) yaw = 0.0f;
        if (b.velocity.x * b.velocity.x + b.velocity.z * b.velocity.z > pl.turnMinSpeed * pl.turnMinSpeed)
            yaw = std::atan2(b.velocity.x, b.velocity.z);
        const Quat facing = quatFromAxisAngle({0, 1, 0}, yaw);
        const Color color = player != kNoPlayer ? look.playerColors[player % net::kMaxPlayers] : pl.aiColor;
        // 밀쳐진 번쩍임: 인스턴스 값(스타일 프리셋)이라 머티리얼과 상관없이 흰색으로 섞인다
        if (!before) view.hitTime[id.slot] = -1.0;
        const f64 sinceHit = viewTime - view.hitTime[id.slot];
        const f32 flash = view.hitTime[id.slot] >= 0.0 && sinceHit < pl.hitFlashSeconds
                              ? pl.hitFlash * (1.0f - static_cast<f32>(sinceHit) / pl.hitFlashSeconds)
                              : 0.0f;
        const u32 flags = render::instanceFlash(0, flash);
        out.add(box, {pos, color.value, facing, pl.bodySize, flags});
        out.add(box, {pos + rotate(facing, pl.visorOffset), pl.visorColor.value, facing, pl.visorSize, flags});
        if (player == localPlayer) { // 내 캐릭터 표시
            const Quat spin = quatFromAxisAngle({0, 1, 0}, static_cast<f32>(now) * pl.markerSpin);
            const f32 size = pl.markerSize;
            out.add(box, {pos + Vec3{0, pl.markerHeight, 0}, pl.markerColor.value, spin, {size, size, size}, 0});
        }
    }
}

} // namespace sandbox
