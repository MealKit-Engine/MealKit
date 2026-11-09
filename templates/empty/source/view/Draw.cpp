#include "view/Draw.h"

#include "mealkit/render/SceneLights.h"

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
    frame.post = render::postParams(models, env.post);
}

void applyLights(const scene::Scene& scene, std::vector<render::PointLight>& lights, render::FrameParams& frame) {
    lights.clear();
    render::addSceneLights(scene, lights);
    frame.lights = lights.data();
    frame.lightCount = static_cast<u32>(lights.size());
}

void applyProbes(const scene::Scene& scene, asset::ModelCache& models, std::vector<render::ReflectionProbe>& probes,
                 render::FrameParams& frame) {
    render::probes(models, scene, probes);
    frame.probes = probes.data();
    frame.probeCount = static_cast<u32>(probes.size());
}

void drawScene(const scene::Scene& scene, asset::ModelCache& models, const Vec3& eye, f64 viewTime, render::DrawList& out) {
    const render::MeshId box = models.boxMesh();
    for (const scene::Entity& e : scene.entities) {
        if (!scene::visibleInGame(e, &reflect::moduleObjectTypes())) continue; // spawn points/areas are invisible in the game
        if (e.is(scene::kTypeScatter)) { // view-only instances painted in the editor
            render::addScatter(out, models, e, eye);
            continue;
        }
        if (e.is(scene::kTypeTerrain)) {
            if (asset::Terrain* terrain = models.terrain(scene::terrainFile(e)))
                render::addTerrain(out, models, *terrain, e.position, e.scale, e.color, e.flags, eye);
            continue;
        }
        const asset::Model* model = e.model.empty() ? nullptr : models.get(e.model);
        const render::MeshId mesh = model ? models.pickLod(*model, eye, e.position, e.scale) : box;
        if (model) render::addModel(out, *model, mesh, {e.position, e.color, e.rotation, e.scale, e.flags}, scene::animField(e), viewTime);
        else out.add(mesh, {e.position, e.color, e.rotation, e.scale, e.flags});
    }
}

void drawWorld(const WorldState& previous, const WorldState& current, f32 alpha, const ViewTuning& look, asset::ModelCache& models,
               render::DrawList& out) {
    const render::MeshId box = models.boxMesh();
    for (auto [id, pawn] : current.pawns) {
        const Pawn* before = previous.pawns.get(id); // nullptr when it appeared this tick
        const Vec3 at = lerp(before ? before->position : pawn.position, pawn.position, alpha);
        out.add(box, {at, look.playerColors[pawn.player % look.playerColors.size()].value, Quat{}, look.pawnSize, 0});
    }
}

} // namespace sandbox
