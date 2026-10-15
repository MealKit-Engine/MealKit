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

// 씬의 충돌 상자(정적), 스폰 지점, 점프대
void loadScene(World& world, const scene::Scene& scene, mealkit::asset::ModelCache& models) {
    u32 padCount = 0;
    for (const scene::Entity& e : scene.entities) padCount += e.is<JumpPad>() ? 1u : 0u;
    world.state.pads.init(padCount);

    for (const scene::Entity& e : scene.entities) {
        if (e.is(scene::kTypeSolid)) {
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
        } else if (e.is(scene::kTypeSpawn)) {
            world.spawns.push_back(e.position);
        } else if (e.is<JumpPad>()) {
            const JumpPad def = scene::readObject<JumpPad>(e);
            Pad pad;
            pad.center = e.position;
            pad.halfExtents = e.scale * 0.5f;
            pad.power = def.power;
            pad.cooldownTicks = def.cooldownTicks;
            world.state.pads.create(pad);
        }
    }
    world.state.pads.endTick();
    world.physics.optimize();
}

} // namespace

void World::reset(u64 seed, u32 playerCount, u32 propCount, u32 propCapacity, const Tuning& tuningValue,
                  const scene::Scene& scene, mealkit::asset::ModelCache& models, u32 physicsThreads) {
    physics.init(16384, physicsThreads);
    tuning = tuningValue;
    physics.setGravity(tuning.gravity);
    spawns.clear();
    state = WorldState{};
    state.playerCount = playerCount;
    state.props.init(propCapacity);
    events.clear();

    // 바디 생성 순서 고정: 씬 → 플레이어 → 상자
    loadScene(*this, scene, models);
    spawnPlayers(*this);
    spawnProps(*this, seed, propCount);
    state.props.endTick(); // 처음 만든 상자도 0틱부터 순회에 나온다
    sensePlayers(*this);
    senseProps(*this);
}

void World::setTuning(const Tuning& value) {
    tuning = value;
    physics.setGravity(tuning.gravity);
}

void World::step(const mealkit::net::TickInputs& tickInputs) {
    MEALKIT_ZONE("World::step");
    for (u32 i = 0; i < state.playerCount; ++i) std::memcpy(&inputs[i], tickInputs.players[i].bytes, sizeof(GameInput));
    events.clear();

    movePlayers(*this);
    usePads(*this);
    physics.step(kTickDt, 2); // 30Hz 틱을 물리 2단계로 나눠 안정성을 높인다
    respawnFallenPlayers(*this);
    recoverFallenProps(*this);
    sensePlayers(*this);
    senseProps(*this);
    // 이번 틱에 지운 상자의 바디를 슬롯 순서로 지운다
    state.props.endTick([this](Prop& prop) { physics.destroy(prop.body); });
    ++state.tick;
}

u64 World::hash() const {
    mealkit::Hasher h;
    mealkit::reflect::hashStruct(mealkit::reflect::typeOf<WorldState>(), &state, h); // 선언한 상태 필드 전부
    physics.hashState(h);
    return h.value;
}

} // namespace sandbox
