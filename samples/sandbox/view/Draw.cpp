#include "view/Draw.h"

#include "mealkit/render/SceneLights.h"

#include <algorithm>
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
    frame.post = models.postParams(env.post);
}

void applyLights(const scene::Scene& scene, const WorldView& view, f64 viewTime, std::vector<render::PointLight>& lights,
                 render::FrameParams& frame) {
    lights.clear();
    render::addSceneLights(scene, lights);
    for (const WorldView::Flash& f : view.flashes) { // brief light at the moment of a push (view-side effect, off after 0.25 s)
        const f32 t = static_cast<f32>(viewTime - f.time) / 0.25f;
        if (t >= 0.0f && t < 1.0f) lights.push_back({f.position, 4.0f, Vec3{1.0f, 0.75f, 0.4f} * (6.0f * (1.0f - t))});
    }
    frame.lights = lights.data();
    frame.lightCount = static_cast<u32>(lights.size());
}

void applyProbes(const scene::Scene& scene, asset::ModelCache& models, std::vector<render::ReflectionProbe>& probes,
                 render::FrameParams& frame) {
    models.probes(scene, probes);
    frame.probes = probes.data();
    frame.probeCount = static_cast<u32>(probes.size());
}

void drawScene(const scene::Scene& scene, asset::ModelCache& models, const Vec3& eye, f64 viewTime, render::DrawList& out) {
    const render::MeshId box = models.boxMesh();
    for (const scene::Entity& e : scene.entities) {
        if (!scene::visibleInGame(e, &reflect::moduleObjectTypes())) continue; // spawn points/areas are invisible in the game
        if (e.is(scene::kTypeScatter)) { // view-only instances painted in the editor
            models.addScatter(out, e, eye);
            continue;
        }
        if (e.is(scene::kTypeTerrain)) {
            if (asset::Terrain* terrain = models.terrain(scene::terrainFile(e)))
                models.addTerrain(out, *terrain, e.position, e.scale, e.color, e.flags, eye);
            continue;
        }
        const asset::Model* model = e.model.empty() ? nullptr : models.get(e.model);
        const render::MeshId mesh = model ? models.pickLod(*model, eye, e.position, e.scale) : box;
        if (model) asset::addModel(out, *model, mesh, {e.position, e.color, e.rotation, e.scale, e.flags}, scene::animField(e), viewTime);
        else out.add(mesh, {e.position, e.color, e.rotation, e.scale, e.flags});
    }
}

void onWorldEvent(const GameEvent& e, const WorldState& state, f64 viewTime, WorldView& view) {
    if (e.kind != EventKind::Push) return;
    std::erase_if(view.flashes, [&](const WorldView::Flash& f) { return viewTime - f.time > 0.25; });
    view.flashes.push_back({e.position + Vec3{0.0f, 1.0f, 0.0f}, viewTime});
    view.hitTime.resize(state.pawns.capacity(), -1.0);
    for (auto [id, pawn] : state.pawns) {
        const Vec3 d = pawn.position - e.position;
        const f32 dist2 = d.x * d.x + d.y * d.y + d.z * d.z;
        if (dist2 > 1e-4f && dist2 <= e.value * e.value) view.hitTime[id.slot] = viewTime; // excluding the pushing Pawn (distance 0)
        else if (dist2 <= 1e-4f && id.slot < view.anims.size()) view.anims[id.slot].params.trigger("push"); // the pusher's animation
    }
}

// One skinned character: the state table's current pose (with its crossfade) into a palette
void drawFigure(const asset::Model& figure, const asset::AnimStates& states, const asset::AnimInstance& anim,
                const render::InstanceData& instance, render::DrawList& out) {
    thread_local std::vector<asset::JointPose> pose;
    thread_local std::vector<asset::BoneMatrix> palette;
    asset::animPose(states, figure.skeleton, figure.clips, anim, pose);
    asset::skinPalette(figure.skeleton, pose, palette);
    out.addSkinned(figure.lods[0], instance, palette.data(), static_cast<u32>(palette.size()));
}

void drawWorld(const WorldState& previous, const WorldState& current, f32 alpha, u8 localPlayer, f64 now, f64 viewTime,
               const ViewTuning& look, asset::ModelCache& models, WorldView& view, render::DrawList& out, const Vec3& localOffset) {
    const PawnLook& pl = look.pawn;
    const render::MeshId box = models.boxMesh();
    const asset::Model* figure = pl.model.empty() ? nullptr : models.get(pl.model);
    if (figure && !figure->skinned()) figure = nullptr;
    view.anims.resize(current.pawns.capacity());
    // Animation advances by view time (stops during hitstop like the rest of the view)
    const f32 dt = view.animTime < 0.0 ? 0.0f : static_cast<f32>(std::max(0.0, viewTime - view.animTime));
    view.animTime = viewTime;
    const i32 set = figure ? view.animStates.findSet(pl.animSet) : -1;
    thread_local std::vector<std::string> events;
    events.clear();
    view.hitTime.resize(current.pawns.capacity(), -1.0);
    for (auto [id, b] : current.props) {
        const Prop* before = previous.props.get(id); // boxes created this tick have no previous state
        const Prop& a = before ? *before : b;
        out.add(box, {lerp(a.position, b.position, alpha), look.propColors[b.colorIndex % kPropColorCount].value,
                      nlerp(a.rotation, b.rotation, alpha), b.halfExtents * 2.0f, 0});
    }
    view.facing.resize(current.pawns.capacity(), 0.0f);
    for (auto [id, b] : current.pawns) {
        const Pawn* before = previous.pawns.get(id); // Pawns created this tick have no previous state
        const Pawn& a = before ? *before : b;
        const u8 player = pawnPlayer(current, b);
        const Vec3 pos = lerp(a.position, b.position, alpha) + (player == localPlayer ? localOffset : Vec3{});
        f32& yaw = view.facing[id.slot];
        if (!before) yaw = 0.0f;
        if (b.velocity.x * b.velocity.x + b.velocity.z * b.velocity.z > pl.turnMinSpeed * pl.turnMinSpeed)
            yaw = std::atan2(b.velocity.x, b.velocity.z);
        const Quat facing = quatFromAxisAngle({0, 1, 0}, yaw);
        const Color color = player != kNoPlayer ? look.playerColors[player % net::kMaxPlayers] : pl.aiColor;
        // Pushed flash: an instance value (style preset), so blended toward white regardless of material
        if (!before) view.hitTime[id.slot] = -1.0;
        const f64 sinceHit = viewTime - view.hitTime[id.slot];
        const f32 flash = view.hitTime[id.slot] >= 0.0 && sinceHit < pl.hitFlashSeconds
                              ? pl.hitFlash * (1.0f - static_cast<f32>(sinceHit) / pl.hitFlashSeconds)
                              : 0.0f;
        const u32 flags = render::instanceFlash(0, flash);
        if (b.mover.mode == sim::MoveMode::Ragdoll) { // active ragdoll: each part as a box from its own body
            const sim::Ragdoll& ra = a.mover.mode == sim::MoveMode::Ragdoll ? a.mover.ragdoll : b.mover.ragdoll;
            const sim::Ragdoll& rb = b.mover.ragdoll;
            const Vec3 offset = player == localPlayer ? localOffset : Vec3{};
            for (u32 i = 0; i < sim::kRagdollParts; ++i)
                out.add(box, {lerp(ra.partPositions[i], rb.partPositions[i], alpha) + offset, color.value,
                              nlerp(ra.partRotations[i], rb.partRotations[i], alpha), sim::ragdollPartSize(i, rb.height), flags});
        } else if (figure) { // skinned character: the state table picks clips from movement parameters (assets/tables/animation.table)
            WorldView::Anim& anim = view.anims[id.slot];
            if (!before) anim.instance = {};
            anim.params.set("speed", std::sqrt(b.velocity.x * b.velocity.x + b.velocity.z * b.velocity.z));
            anim.params.set("grounded", b.mover.grounded != 0);
            const size_t firstEvent = events.size();
            asset::updateAnim(view.animStates, set, figure->clips, anim.instance, anim.params, dt, &events);
            for (size_t i = firstEvent; i < events.size(); ++i) view.animEvents.push_back({std::move(events[i]), pos});
            drawFigure(*figure, view.animStates, anim.instance, {pos - Vec3{0, pl.footOffset, 0}, color.value, facing, {1, 1, 1}, flags}, out);
        } else {
            out.add(box, {pos, color.value, facing, pl.bodySize, flags});
            out.add(box, {pos + rotate(facing, pl.visorOffset), pl.visorColor.value, facing, pl.visorSize, flags});
        }
        if (player == localPlayer) { // my character indicator
            const Quat spin = quatFromAxisAngle({0, 1, 0}, static_cast<f32>(now) * pl.markerSpin);
            const f32 size = pl.markerSize;
            out.add(box, {pos + Vec3{0, pl.markerHeight, 0}, pl.markerColor.value, spin, {size, size, size}, 0});
        }
    }
}

} // namespace sandbox
