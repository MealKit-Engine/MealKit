#include "sim/World.h"

#include "sim/Systems.h"

#include "mealkit/core/Hash.h"
#include "mealkit/core/Profile.h"

#include <cstring>

namespace sandbox {
namespace {

using mealkit::physics::BodyDesc;
using mealkit::physics::Motion;
using mealkit::scene::EntityKind;

constexpr Vec3 kGravity{0.0f, -20.0f, 0.0f};

// 씬의 충돌 상자(정적)와 스폰 지점
void loadScene(World& world, const mealkit::scene::Scene& scene, mealkit::asset::ModelCache& models) {
    for (const mealkit::scene::Entity& e : scene.entities) {
        if (e.kind == EntityKind::Solid) {
            BodyDesc d;
            d.motion = Motion::Static;
            d.position = e.position;
            d.rotation = e.rotation;
            d.friction = 0.8f;
            const mealkit::asset::Model* model = e.model.empty() ? nullptr : models.get(e.model);
            if (model) {
                std::vector<Vec3> points;
                points.reserve(model->hullPoints.size());
                for (const Vec3& p : model->hullPoints) points.push_back({p.x * e.scale.x, p.y * e.scale.y, p.z * e.scale.z});
                world.physics.createConvexHull(points.data(), static_cast<u32>(points.size()), d);
            } else {
                world.physics.createBox(e.scale * 0.5f, d);
            }
        } else if (e.kind == EntityKind::SpawnPoint) {
            world.spawns.push_back(e.position);
        }
    }
    world.physics.optimize();
}

} // namespace

void World::reset(u64 seed, u32 playerCount, u32 propCount, const mealkit::scene::Scene& scene,
                  mealkit::asset::ModelCache& models, u32 physicsThreads) {
    physics.init(16384, physicsThreads);
    physics.setGravity(kGravity);
    spawns.clear();
    propBodies.clear();
    state = WorldState{};
    state.playerCount = playerCount;

    // 바디 생성 순서 고정: 씬 → 플레이어 → 상자
    loadScene(*this, scene, models);
    spawnPlayers(*this);
    spawnProps(*this, seed, propCount);
    sensePlayers(*this);
    senseProps(*this);
}

void World::step(const mealkit::net::TickInputs& tickInputs) {
    MEALKIT_ZONE("World::step");
    for (u32 i = 0; i < state.playerCount; ++i) std::memcpy(&inputs[i], tickInputs.players[i].bytes, sizeof(GameInput));

    movePlayers(*this);
    physics.step(kTickDt, 2); // 30Hz 틱을 물리 2단계로 나눠 안정성을 높인다
    respawnFallenPlayers(*this);
    recoverFallenProps(*this);
    sensePlayers(*this);
    senseProps(*this);
    ++state.tick;
}

u64 World::hash() const {
    mealkit::Hasher h;
    h.add(state.tick);
    for (u32 i = 0; i < state.playerCount; ++i) {
        h.add(state.players[i].pushCooldown);
        h.add(state.players[i].grounded);
    }
    physics.hashState(h);
    return h.value;
}

} // namespace sandbox
