#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/render/ModelDraw.h" // asset::ModelCache + its drawing
#include "mealkit/render/DrawList.h"
#include "mealkit/render/Renderer.h"
#include "mealkit/render/Sprites.h"
#include "mealkit/scene/Scene.h"

#include <array>
#include <vector>

// Scene drawing. Simulation state is only read.
namespace sandbox {

// Scene environment → view (sun, sky, fog, HDRI, post effects)
void applyEnvironment(const mealkit::scene::Environment& env, mealkit::asset::ModelCache& models, mealkit::render::FrameParams& frame);
// Scene point lights and reflection probes → view (frame points at the vectors: they must live for the frame)
void applyLights(const mealkit::scene::Scene& scene, std::vector<mealkit::render::PointLight>& lights, mealkit::render::FrameParams& frame);
void applyProbes(const mealkit::scene::Scene& scene, mealkit::asset::ModelCache& models,
                 std::vector<mealkit::render::ReflectionProbe>& probes, mealkit::render::FrameParams& frame);
// Static scene objects (spawn points and areas aren't drawn), sprites included
void drawScene(const mealkit::scene::Scene& scene, mealkit::asset::ModelCache& models, mealkit::render::SpriteTextures& sprites, const mealkit::render::SpriteClips& clips, const Vec3& eye, mealkit::f64 viewTime,
               mealkit::render::DrawList& out);
// Which way each player's character faces (kept by the view between frames: the simulation has no facing)
struct PawnFacing {
    std::array<bool, mealkit::session::kMaxPlayers> left{};
};
// Characters interpolated by alpha between the previous and current tick, drawn as sprites (hero_idle / hero_run clips)
void drawWorld(const WorldState& previous, const WorldState& current, f32 alpha, const ViewTuning& look, mealkit::asset::ModelCache& models,
               mealkit::render::SpriteTextures& sprites, const mealkit::render::SpriteClips& clips, mealkit::f64 viewTime, PawnFacing& facing,
               mealkit::render::DrawList& out);

} // namespace sandbox
