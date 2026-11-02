#include "sim/World.h"

#include "sim/Systems.h"

#include "mealkit/core/Hash.h"
#include "mealkit/core/Log.h"
#include "mealkit/core/Profile.h"

#include <cstring>

namespace coop {
namespace {

using mealkit::physics::BodyDesc;
using mealkit::physics::Motion;
namespace scene = mealkit::scene;

// The scene's static collision (boxes, model hulls, terrain heightfield), spawn points, match area, goal, chaser spawns
void loadScene(World& world, const scene::Scene& scene, mealkit::asset::ModelCache& models) {
    bool arena = false;
    for (const scene::Entity& e : scene.entities) {
        if (e.is(scene::kTypeSolid)) {
            BodyDesc d;
            d.motion = Motion::Static;
            d.position = e.position;
            d.rotation = e.rotation;
            d.friction = world.tuning.solidFriction;
            const mealkit::asset::Model* model = e.model.empty() ? nullptr : models.get(e.model);
            if (model) {
                std::vector<Vec3> points;
                points.reserve(model->hullPoints.size());
                for (const Vec3& p : model->hullPoints) points.push_back({p.x * e.scale.x, p.y * e.scale.y, p.z * e.scale.z});
                world.physics.createConvexHull(points.data(), static_cast<u32>(points.size()), d);
            } else {
                world.physics.createBox(e.scale * 0.5f, d);
            }
        } else if (e.is(scene::kTypeTerrain)) { // heightmap (rotation ignored: terrain is axis-aligned)
            const mealkit::asset::Terrain* terrain = models.terrain(scene::terrainFile(e));
            if (!terrain) continue;
            const mealkit::asset::Heightmap& map = terrain->map;
            std::vector<f32> heights;
            mealkit::asset::terrainHeights(map, e.scale.y, heights);
            BodyDesc d;
            d.position = e.position;
            d.friction = world.tuning.solidFriction;
            const f32 cells = static_cast<f32>(map.size - 1);
            world.physics.createHeightField(heights.data(), map.size, e.scale * -0.5f, e.scale.x / cells, e.scale.z / cells, d);
        } else if (e.is(scene::kTypeSpawn)) {
            world.spawns.push_back(e.position);
        } else if (e.is<ChaserSpawn>()) {
            world.chaserSpawns.push_back({e.position, scene::readObject<ChaserSpawn>(e).count});
        } else if (e.is<MatchArea>() && !arena) {
            const scene::Bounds b = scene::areaBounds(e);
            world.arenaMin = b.min;
            world.arenaMax = b.max;
            arena = true;
        } else if (e.is<GoalArea>() && !world.hasGoal) {
            const scene::Bounds b = scene::areaBounds(e);
            world.goalMin = b.min;
            world.goalMax = b.max;
            world.hasGoal = true;
        }
    }
    if (!arena) {
        MEALKIT_WARN("Scene has no MatchArea - using a default 80 m area");
        world.arenaMin = {-40.0f, -30.0f, -40.0f}; // const-ok: safe value for scenes without an area
        world.arenaMax = {40.0f, 30.0f, 40.0f};    // const-ok: safe value for scenes without an area
    }
    if (!world.hasGoal) MEALKIT_WARN("Scene has no GoalArea - the round only ends by time or when everyone is down");
    world.physics.optimize();
}

} // namespace

void World::reset(u64 seed, u32 playerCount, const Tuning& tuningValue, const mealkit::sim::Table<BodyKind>& bodyKindTable,
                  const std::vector<u32>& playerBodies, const scene::Scene& scene, mealkit::asset::ModelCache& models,
                  u32 tickRateValue, u32 physicsThreads) {
    physics.init(4096, physicsThreads);
    tickRate = tickRateValue ? tickRateValue : 30u;
    tickDt = 1.0f / static_cast<f32>(tickRate);
    tuning = tuningValue;
    rules = matchRules(tuning);
    bodyKinds = bodyKindTable;
    physics.setContactEvents(tuning.impactSpeed, tuning.impactsPerTick);
    physics.setGravity(tuning.gravity);
    spawns.clear();
    chaserSpawns.clear();
    hasGoal = false;
    state = WorldState{};
    events.clear();

    // Fixed body creation order: scene → Pawns (heroes in slot order, then chasers)
    loadScene(*this, scene, models);
    nav = mealkit::sim::bakeNavGrid(physics, arenaMin, arenaMax, tuning.nav); // only static scene bodies exist now
    state.playerCount = playerCount;
    for (u32 i = 0; i < playerCount; ++i)
        state.slots[i].body = i < playerBodies.size() && playerBodies[i] < bodyKinds.size() ? playerBodies[i] : 0u;
    u32 chasers = 0;
    for (const ChaserSpawn& s : chaserSpawns) chasers += s.count;
    state.controllers.init(playerCount + chasers);
    state.pawns.init((playerCount + chasers) * 2); // a deleted Pawn and its replacement can coexist within one tick
    state.aiRng = mealkit::Pcg32(seed ^ 0x9E3779B97F4A7C15ull, 7);

    createControllers(*this);
    state.controllers.endTick();
    respawnPawns(*this); // every controller starts with a 0 respawn wait: first Pawns
    state.pawns.endTick();
    sensePawns(*this);
    startMatch(*this);
    events.clear(); // spawn events of Pawns created at start aren't sent
}

void World::setTuning(const Tuning& value) {
    tuning = value;
    rules = matchRules(tuning);
    physics.setGravity(tuning.gravity);
}

void World::step(const mealkit::net::TickInputs& tickInputs) {
    MEALKIT_ZONE("World::step");
    for (u32 i = 0; i < state.playerCount; ++i) std::memcpy(&inputs[i], tickInputs.players[i].bytes, sizeof(GameInput));
    events.clear();

    applyCommands(*this, tickInputs);
    updateMatch(*this); // waiting → countdown → playing (goal check) → round end → results
    respawnPawns(*this);
    controlPawns(*this);
    physics.step(tickDt, 2); // 2 physics substeps per tick (ragdolls)
    for (const mealkit::physics::ContactEvent& c : physics.contacts())
        events.push(makeEvent(EventKind::Impact, kNoPlayer, c.point, c.speed));
    removeFallenPawns(*this);
    sensePawns(*this);
    catchHeroes(*this);
    reviveHeroes(*this);
    state.controllers.endTick();
    state.pawns.endTick([this](Pawn& pawn) { mealkit::sim::destroyMover(physics, pawn.mover); });
    ++state.tick;
}

u64 World::hash() const {
    mealkit::Hasher h;
    mealkit::reflect::hashStruct(mealkit::reflect::typeOf<WorldState>(), &state, h);
    physics.hashState(h);
    return h.value;
}

} // namespace coop
