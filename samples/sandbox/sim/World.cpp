#include "sim/World.h"

#include "sim/Systems.h"

#include "mealkit/core/Hash.h"
#include "mealkit/core/Log.h"
#include "mealkit/core/Profile.h"

#include <cstring>

namespace sandbox {
namespace {

using mealkit::physics::BodyDesc;
using mealkit::physics::Motion;
namespace scene = mealkit::scene;

// 씬의 충돌 상자(정적), 스폰 지점, 점프대, 경기 영역, 상자 생성 영역
void loadScene(World& world, const scene::Scene& scene, mealkit::asset::ModelCache& models) {
    u32 padCount = 0;
    for (const scene::Entity& e : scene.entities) padCount += e.is<JumpPad>() ? 1u : 0u;
    world.state.pads.init(padCount);
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
                for (const Vec3& p : model->hullPoints)
                    points.push_back({p.x * e.scale.x, p.y * e.scale.y, p.z * e.scale.z});
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
            pad.cooldownTicks = def.cooldown;
            world.state.pads.create(pad);
        } else if (e.is<MatchArea>() && !arena) {
            const scene::Bounds b = scene::areaBounds(e);
            world.arenaMin = b.min;
            world.arenaMax = b.max;
            arena = true;
        } else if (e.is<PropSpawnArea>()) {
            const scene::Bounds b = scene::areaBounds(e);
            const PropSpawnArea def = scene::readObject<PropSpawnArea>(e);
            u32 kind = world.propKinds.find(def.kind.id); // 행 이름 → 번호 (불러올 때 한 번)
            if (kind == mealkit::kNoRow) {
                MEALKIT_WARN("%s: box kind '%s' is not in tables/props.table - using the first row", e.name.c_str(),
                             def.kind.id.c_str());
                kind = 0;
            }
            if (world.propKinds.size() == 0) continue; // 상자 종류가 없으면 상자도 없다
            world.propSpawns.push_back({b.min, b.max, def.count, kind});
        }
    }
    if (!arena) { // 경기 영역이 없는 씬 (엔진 기본 아레나 등): 예전 sandbox 값으로 돈다
        MEALKIT_WARN("Scene has no MatchArea - using a default 80 m arena");
        world.arenaMin = {-40.0f, -30.0f, -40.0f}; // const-ok: 영역이 없는 씬의 안전값
        world.arenaMax = {40.0f, 30.0f, 40.0f};    // const-ok: 영역이 없는 씬의 안전값
    }
    world.state.pads.endTick();
    world.physics.optimize();
}

// 상자 수를 override로 맞춘다: 영역마다 비율대로, 나머지는 마지막 영역 (정수 계산이라 모든 피어가 같다)
void overridePropCount(std::vector<World::PropSpawn>& spawns, u32 total) {
    u64 declared = 0;
    for (const World::PropSpawn& s : spawns) declared += s.count;
    if (spawns.empty() || declared == 0) return;
    u32 given = 0;
    for (World::PropSpawn& s : spawns) {
        s.count = static_cast<u32>(static_cast<u64>(s.count) * total / declared);
        given += s.count;
    }
    spawns.back().count += total - given;
}

} // namespace

void World::reset(u64 seed, u32 playerCount, u32 propOverride, const Tuning& tuningValue,
                  const mealkit::sim::Table<PropKind>& propKindTable, const mealkit::sim::Table<BodyKind>& bodyKindTable,
                  const std::vector<u32>& playerBodies, const scene::Scene& scene, mealkit::asset::ModelCache& models,
                  u32 physicsThreads) {
    physics.init(16384, physicsThreads);
    tuning = tuningValue;
    propKinds = propKindTable;
    bodyKinds = bodyKindTable;
    physics.setContactEvents(tuning.impactSpeed, tuning.impactsPerTick); // 충돌 소리 (화면 알림)
    physics.setGravity(tuning.gravity);
    spawns.clear();
    propSpawns.clear();
    state = WorldState{};
    events.clear();

    // 바디 생성 순서 고정: 씬 → Pawn → 상자. 씬을 먼저 읽어야 상자 수(= Pool 용량, 물리 바디 수)를 안다.
    loadScene(*this, scene, models);
    if (propOverride) overridePropCount(propSpawns, propOverride);
    u32 propCount = 0;
    for (const PropSpawn& s : propSpawns) propCount += s.count;
    state.playerCount = playerCount;
    for (u32 i = 0; i < playerCount; ++i) // 매치 시작 데이터: 없는 행이면 첫 행
        state.slots[i].body = i < playerBodies.size() && playerBodies[i] < bodyKinds.size() ? playerBodies[i] : 0u;
    const u32 controllers = playerCount + tuning.aiPawns;
    state.controllers.init(controllers);
    state.pawns.init(controllers * 2); // 리스폰 대기 0틱이면 지우는 Pawn과 새 Pawn이 한 틱에 같이 있다
    state.props.init(propCount);
    state.aiRng = mealkit::Pcg32(seed ^ 0x9E3779B97F4A7C15ull, 7);

    createControllers(*this);
    state.controllers.endTick();
    respawnPawns(*this); // 리스폰 대기가 0인 컨트롤러 = 전부: 첫 Pawn
    state.pawns.endTick();
    spawnProps(*this, seed);
    state.props.endTick(); // 처음 만든 상자도 0틱부터 순회에 나온다
    sensePawns(*this);
    senseProps(*this);
    events.clear(); // 시작 때 만든 Pawn의 스폰 이벤트는 보내지 않는다
}

void World::setTuning(const Tuning& value) {
    tuning = value;
    physics.setGravity(tuning.gravity);
}

void World::step(const mealkit::net::TickInputs& tickInputs) {
    MEALKIT_ZONE("World::step");
    for (u32 i = 0; i < state.playerCount; ++i) std::memcpy(&inputs[i], tickInputs.players[i].bytes, sizeof(GameInput));
    events.clear();

    applyCommands(*this, tickInputs); // 확정 명령 (디버그 치트 등): 플레이어 번호 → 보낸 순서
    respawnPawns(*this);
    controlPawns(*this);
    usePads(*this);
    physics.step(kTickDt, 2); // 30Hz 틱을 물리 2단계로 나눠 안정성을 높인다
    for (const mealkit::physics::ContactEvent& c : physics.contacts()) // 센 것부터 (스레드 수와 무관한 순서)
        events.push(makeEvent(EventKind::Impact, kNoPlayer, c.point, c.speed));
    removeFallenPawns(*this);
    recoverFallenProps(*this);
    sensePawns(*this);
    senseProps(*this);
    // 틱 끝: 이번 틱에 지운 객체의 바디를 슬롯 순서로 지우고, 이번 틱에 만든 객체를 다음 틱부터 순회에 넣는다
    state.controllers.endTick();
    state.pawns.endTick([this](Pawn& pawn) { physics.destroy(pawn.body); });
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
