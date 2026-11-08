#include "sim/World.h"

#include "sim/Systems.h"

#include "mealkit/core/Hash.h"
#include "mealkit/core/Profile.h"

#include <cstring>

namespace sandbox {
namespace {

using mealkit::physics::BodyDesc;
using mealkit::physics::Motion;
namespace scene = mealkit::scene;

// The scene's collision objects (static: box, or the model's convex hull), terrain, spawn points.
// Game object kinds (sim/Objects.h) are read here with scene::readObject<T>(entity).
void loadScene(World& world, const scene::Scene& scene, mealkit::asset::ModelCache& models) {
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
                for (const Vec3& p : model->hullPoints)
                    points.push_back({p.x * e.scale.x, p.y * e.scale.y, p.z * e.scale.z});
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
        }
    }
    world.physics.optimize();
}

} // namespace

void World::reset(u64 seed, u32 playerCount, const Tuning& tuningValue, const scene::Scene& scene,
                  mealkit::asset::ModelCache& models, u32 tickRateValue, u32 physicsThreads) {
    (void)seed; // the match seed: start random sequences from it (mealkit::Pcg32 state field)
    physics.init(4096, physicsThreads);
    tickRate = tickRateValue ? tickRateValue : 30u;
    tickDt = 1.0f / static_cast<f32>(tickRate);
    tuning = tuningValue;
    rules = matchRules(tuning);
    physics.setGravity(tuning.gravity);
    spawns.clear();
    state = WorldState{};
    events.clear();

    // Fixed body creation order: scene → characters
    loadScene(*this, scene, models);
    state.playerCount = playerCount;
    for (u32 i = 0; i < playerCount; ++i) state.slots[i].connected = true;
    state.pawns.init(mealkit::session::kMaxPlayers * 2); // with a 0-tick respawn wait, the old and new character coexist within one tick
    respawnPawns(*this);
    state.pawns.endTick();
    sensePawns(*this);
    startMatch(*this);
    events.clear(); // spawn events of the first characters aren't sent
}

void World::setTuning(const Tuning& value) {
    tuning = value;
    rules = matchRules(tuning);
    physics.setGravity(tuning.gravity);
}

void World::step(const mealkit::session::TickInputs& tickInputs) {
    MEALKIT_ZONE("World::step");
    for (u32 i = 0; i < state.playerCount; ++i) std::memcpy(&inputs[i], tickInputs.players[i].bytes, sizeof(GameInput));
    events.clear();

    applyCommands(*this, tickInputs); // confirmed commands: player index → send order
    updateMatch(*this);               // waiting → countdown → playing → round end → results
    respawnPawns(*this);
    movePawns(*this);
    physics.step(tickDt, 2); // splits a tick into 2 physics substeps for stability
    removeFallenPawns(*this);
    sensePawns(*this);
    // End of tick: delete bodies of characters removed this tick, and put ones created this tick into iteration from the next tick
    state.pawns.endTick([this](Pawn& pawn) { mealkit::sim::destroyMover(physics, pawn.mover); });
    ++state.tick;
}

u64 World::hash() const {
    mealkit::Hasher h;
    mealkit::reflect::hashStruct(mealkit::reflect::typeOf<WorldState>(), &state, h); // all declared state fields
    physics.hashState(h);
    return h.value;
}

} // namespace sandbox
