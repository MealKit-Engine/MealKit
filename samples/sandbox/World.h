#pragma once

#include "aster/asset/ModelCache.h"
#include "aster/core/Math.h"
#include "aster/net/Lockstep.h"
#include "aster/physics/PhysicsWorld.h"
#include "aster/scene/Scene.h"

#include <vector>

// 샌드박스 게임의 결정론적 시뮬레이션 (물리: Jolt).
// 규칙: 이 파일과 World.cpp에서는 sin/cos/atan2 등 CRT 수학 함수, 렌더링 상태, 실제 시간을 쓰지 않는다.
// 물리 바디는 항상 같은 순서로 만들고, 힘도 같은 순서로 준다.
namespace sandbox {

using aster::f32;
using aster::i8;
using aster::u32;
using aster::u64;
using aster::u8;
using aster::Quat;
using aster::Vec3;

inline constexpr f32 kTickRate = 30.0f;
inline constexpr f32 kTickDt = 1.0f / kTickRate;
inline constexpr f32 kKillPlaneY = -30.0f;
inline constexpr f32 kPlayerHalfHeight = 0.9f; // 캡슐 전체 높이 1.8m
inline constexpr f32 kPlayerRadius = 0.45f;

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
static_assert(sizeof(GameInput) == aster::net::kInputBytes);

struct Player {
    Vec3 position;
    Vec3 velocity;
    u32 pushCooldown = 0; // 틱 단위 (실수 타이머 대신 정수)
    u32 grounded = 0;     // 발밑에 무언가 있는가
};

struct Body {
    Vec3 position;
    Quat rotation;
    Vec3 halfExtents;
    u32 color = 0;
};

// 렌더링과 보간에 쓰는 틱별 스냅샷. 물리의 실제 상태는 PhysicsWorld 안에 있다.
struct WorldState {
    u32 tick = 0;
    u32 playerCount = 0;
    Player players[aster::net::kMaxPlayers];
    std::vector<Body> bodies;
};

class World {
public:
    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // physicsThreads = 0 이면 코어 수에 맞춘다 (결과는 스레드 수와 무관하다).
    // 모델이 있는 충돌 물체는 모델 정점의 볼록 껍질로 충돌한다.
    void reset(u64 seed, u32 playerCount, u32 bodyCount, const aster::scene::Scene& scene, aster::asset::ModelCache& models,
               u32 physicsThreads = 0);
    void step(const aster::net::TickInputs& inputs);
    u64 hash() const;
    const WorldState& state() const { return m_state; }
    u32 activeBodies() const { return m_physics.activeBodyCount(); }

private:
    void push(const Vec3& center);
    void syncState();
    Vec3 spawnPosition(u32 player) const;

    WorldState m_state;
    aster::physics::PhysicsWorld m_physics;
    std::vector<aster::physics::BodyId> m_bodyIds;
    aster::physics::BodyId m_playerIds[aster::net::kMaxPlayers] = {};
    std::vector<Vec3> m_spawns;
};

} // namespace sandbox
