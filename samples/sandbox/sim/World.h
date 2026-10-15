#pragma once

#include "mealkit/asset/ModelCache.h"
#include "mealkit/core/Math.h"
#include "mealkit/net/Lockstep.h"
#include "mealkit/physics/PhysicsWorld.h"
#include "mealkit/scene/Scene.h"
#include "mealkit/sim/Pool.h"
#include "mealkit/sim/TickEvents.h"
#include "mealkit/sim/TickTimer.h"
#include "sim/Objects.h"

#include <vector>

// 샌드박스 게임의 결정론적 시뮬레이션 (물리: Jolt).
// sim/ 폴더 규칙 (빌드 때 SimCheck가 검사한다): CRT 삼각함수·실제 시간·rand·렌더러/플랫폼 헤더·전역 가변 상태 금지.
// 물리 바디는 항상 같은 순서로 만들고, 힘도 같은 순서로 준다. 새 상태는 World::hash()에 넣는다.
// 매치 중에 생기고 사라지는 객체는 Pool<T>, "N틱 뒤"는 TickTimer, 화면에 알릴 일은 world.events (docs/CODING_MODEL.md).
namespace sandbox {

using mealkit::f32;
using mealkit::i8;
using mealkit::u32;
using mealkit::u64;
using mealkit::u8;
using mealkit::Quat;
using mealkit::Vec3;

inline constexpr f32 kTickRate = 30.0f;
inline constexpr f32 kTickDt = 1.0f / kTickRate;
inline constexpr f32 kKillPlaneY = -30.0f;
inline constexpr f32 kPlayerHalfHeight = 0.9f; // 캡슐 전체 높이 1.8m
inline constexpr f32 kPlayerRadius = 0.45f;
inline constexpr f32 kPushRadius = 7.0f; // 밀치기 범위 (화면 효과도 같은 값을 쓴다)
inline constexpr u32 kPropColorCount = 6; // 색 자체는 화면 쪽(view/Draw.cpp)이 정한다

inline constexpr u8 kButtonJump = 1;
inline constexpr u8 kButtonPush = 2;

// 네트워크로 전달되는 플레이어 입력 (8바이트).
// 이동 방향은 클라이언트가 카메라 기준으로 계산해 월드 방향으로 양자화한다 → 시뮬레이션에 삼각함수가 필요 없다.
struct GameInput {
    i8 moveX = 0; // -127..127
    i8 moveZ = 0;
    u8 buttons = 0;
    u8 reserved[5] = {};
};
static_assert(sizeof(GameInput) == mealkit::net::kInputBytes);

struct Player {
    Vec3 position;
    Vec3 velocity;
    mealkit::sim::TickTimer pushReady; // 다음 밀치기를 할 수 있는 틱
    u32 grounded = 0;     // 발밑에 무언가 있는가
};

// 흩어진 상자
struct Prop {
    Vec3 position;
    Quat rotation;
    Vec3 halfExtents;
    u32 colorIndex = 0; // 0..kPropColorCount-1
    mealkit::physics::BodyId body = mealkit::physics::kInvalidBody;
};

// 씬의 점프대 (매치 시작 때 씬에서 만든다)
struct Pad {
    Vec3 center;
    Vec3 halfExtents;
    f32 power = 0.0f;
    u32 cooldownTicks = 0;
    mealkit::sim::TickTimer ready;
};

// 시뮬레이션 → 화면 알림 (소리·효과용). sim/은 쓰기만 하고 읽지 않는다.
enum class EventKind : u8 { Push, Bounce };

struct GameEvent {
    EventKind kind = EventKind::Push;
    u8 player = 0;
    Vec3 position;
};
inline constexpr u32 kMaxEventsPerTick = 64;

// 렌더링과 보간에 쓰는 틱별 스냅샷. 물리의 실제 상태는 PhysicsWorld 안에 있다.
struct WorldState {
    u32 tick = 0;
    u32 playerCount = 0;
    Player players[mealkit::net::kMaxPlayers];
    mealkit::sim::Pool<Prop> props; // 위치·회전은 물리 결과를 매 틱 옮겨 담은 것
    mealkit::sim::Pool<Pad> pads;
};

// 시뮬레이션 전체. sim/ 시스템 함수(Systems.h)는 이것 하나(World&)만 받는다.
struct World {
    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // physicsThreads = 0 이면 코어 수에 맞춘다 (결과는 스레드 수와 무관하다).
    // 모델이 있는 충돌 물체는 모델 정점의 볼록 껍질로 충돌한다.
    // propCapacity: 상자 Pool 용량 (propCount 이상). 매치 중에는 바뀌지 않는다.
    void reset(u64 seed, u32 playerCount, u32 propCount, u32 propCapacity, const mealkit::scene::Scene& scene,
               mealkit::asset::ModelCache& models, u32 physicsThreads = 0);
    void step(const mealkit::net::TickInputs& inputs);
    u64 hash() const;
    u32 activeBodies() const { return physics.activeBodyCount(); }

    // 매치 시작 때 정해지는 데이터
    std::vector<Vec3> spawns;

    // 상태
    WorldState state;
    GameInput inputs[mealkit::net::kMaxPlayers] = {}; // 이번 틱 입력 (step 시작 때 채운다)
    mealkit::physics::PhysicsWorld physics;
    mealkit::physics::BodyId playerBodies[mealkit::net::kMaxPlayers] = {};

    // 이번 틱의 화면 알림 (step 시작 때 비운다, 해시에 넣지 않는다)
    mealkit::sim::TickEvents<GameEvent, kMaxEventsPerTick> events;
};

} // namespace sandbox
