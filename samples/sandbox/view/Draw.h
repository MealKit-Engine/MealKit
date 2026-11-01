#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/asset/AnimStates.h"
#include "mealkit/asset/ModelCache.h"
#include "mealkit/render/DrawList.h"
#include "mealkit/render/Renderer.h"
#include "mealkit/scene/Scene.h"

#include <vector>

// Scene drawing. Simulation state is only read.
namespace sandbox {

// State that exists only in the view (kept between frames)
struct WorldView {
    std::vector<f32> facing;      // facing direction per Pawn slot (radians)
    std::vector<mealkit::f64> hitTime; // view time each Pawn slot was last pushed (flash, negative if none)
    struct Flash {
        Vec3 position;
        mealkit::f64 time;
    };
    std::vector<Flash> flashes; // push lights (view time; applyLights turns them into point lights)
    // Skinned character animation (per Pawn slot): state table playback (assets/tables/animation.table) and the parameters it reads
    struct Anim {
        mealkit::asset::AnimInstance instance;
        mealkit::asset::AnimParams params;
    };
    std::vector<Anim> anims;
    mealkit::asset::AnimStates animStates; // loaded by GameSession, reloaded when the file changes
    mealkit::f64 animTime = -1.0;          // view time of the last animation update (negative = none yet)
    struct AnimEvent {
        std::string row; // feedback table row
        Vec3 position;
    };
    std::vector<AnimEvent> animEvents; // fired by drawWorld this frame; GameSession plays them through ctx.feedback
};

// Tick events → view state: other Pawns within push range flash (viewTime: the view clock that stops during hitstop)
void onWorldEvent(const GameEvent& e, const WorldState& state, mealkit::f64 viewTime, WorldView& view);

// Scene environment → view (the sky HDRI index is given by models after baking or reading from cache)
void applyEnvironment(const mealkit::scene::Environment& env, mealkit::asset::ModelCache& models, mealkit::render::FrameParams& frame);
// Scene point lights + push lights → view (frame points at lights: they must live for the frame)
void applyLights(const mealkit::scene::Scene& scene, const WorldView& view, mealkit::f64 viewTime,
                 std::vector<mealkit::render::PointLight>& lights, mealkit::render::FrameParams& frame);
// Scene reflection probes → view (frame points at probes: they must live for the frame)
void applyProbes(const mealkit::scene::Scene& scene, mealkit::asset::ModelCache& models,
                 std::vector<mealkit::render::ReflectionProbe>& probes, mealkit::render::FrameParams& frame);
// Static scene objects (spawn points and areas aren't drawn)
void drawScene(const mealkit::scene::Scene& scene, mealkit::asset::ModelCache& models, const Vec3& eye, mealkit::f64 viewTime,
               mealkit::render::DrawList& out);
// Draws boxes and Pawns interpolated by alpha between the previous and current tick. localOffset: only my character is drawn moved further (prediction)
void drawWorld(const WorldState& previous, const WorldState& current, f32 alpha, u8 localPlayer, mealkit::f64 now,
               mealkit::f64 viewTime, const ViewTuning& look, mealkit::asset::ModelCache& models, WorldView& view, mealkit::render::DrawList& out,
               const Vec3& localOffset = {});

} // namespace sandbox
