// 플레이어 캡슐: 생성, 이동, 점프, 밀치기, 떨어지면 리스폰.
#include "sim/Systems.h"

#include <algorithm>
#include <cmath>

namespace sandbox {
namespace {

constexpr f32 kPlayerSpeed = 7.0f;
constexpr f32 kJumpSpeed = 8.0f;
constexpr u32 kPushCooldownTicks = 15;

// 씬에 스폰 지점이 없을 때 쓰는 기본 위치 (삼각함수 대신 고정 테이블)
constexpr Vec3 kDefaultSpawns[mealkit::net::kMaxPlayers] = {
    {-6, kPlayerHalfHeight, -6}, {6, kPlayerHalfHeight, 6},  {6, kPlayerHalfHeight, -6}, {-6, kPlayerHalfHeight, 6},
    {0, kPlayerHalfHeight, -9},  {0, kPlayerHalfHeight, 9},  {-9, kPlayerHalfHeight, 0}, {9, kPlayerHalfHeight, 0},
};

} // namespace

Vec3 spawnPosition(const World& world, u32 player) {
    if (world.spawns.empty()) return kDefaultSpawns[player % mealkit::net::kMaxPlayers];
    const u32 n = static_cast<u32>(world.spawns.size());
    const f32 offset = static_cast<f32>(player / n) * 1.2f; // 스폰 지점보다 사람이 많으면 옆으로 비켜 선다
    return world.spawns[player % n] + Vec3{offset, 0.0f, 0.0f};
}

// 회전이 잠긴 캡슐. 속도를 직접 정해 움직이고, 물리적으로 상자를 밀어낸다.
void spawnPlayers(World& world) {
    for (u32 i = 0; i < world.state.playerCount; ++i) {
        mealkit::physics::BodyDesc d;
        d.position = spawnPosition(world, i);
        d.lockRotation = true;
        d.friction = 0.0f;
        d.restitution = 0.0f;
        d.linearDamping = 0.0f;
        d.mass = 80.0f;
        d.allowSleep = false;
        world.playerBodies[i] = world.physics.createCapsule(kPlayerHalfHeight - kPlayerRadius, kPlayerRadius, d);
    }
}

void movePlayers(World& world) {
    for (u32 i = 0; i < world.state.playerCount; ++i) { // 플레이어 번호 순서
        const GameInput& in = world.inputs[i];
        Player& p = world.state.players[i];
        const mealkit::physics::BodyId id = world.playerBodies[i];

        Vec3 wish{static_cast<f32>(in.moveX) / 127.0f, 0.0f, static_cast<f32>(in.moveZ) / 127.0f};
        const f32 wishLen2 = mealkit::lengthSq(wish);
        if (wishLen2 > 1.0f) wish = wish / std::sqrt(wishLen2);

        Vec3 v = world.physics.linearVelocity(id);
        const f32 accel = std::min(1.0f, (p.grounded ? 12.0f : 3.0f) * kTickDt);
        v.x += (wish.x * kPlayerSpeed - v.x) * accel;
        v.z += (wish.z * kPlayerSpeed - v.z) * accel;
        if (p.grounded && (in.buttons & kButtonJump) && v.y < 1.0f) v.y = kJumpSpeed;
        world.physics.setLinearVelocity(id, v);

        if ((in.buttons & kButtonPush) && p.pushReady.ready(world.state.tick)) {
            p.pushReady.start(world.state.tick, kPushCooldownTicks);
            pushProps(world, p.position);
            world.events.push({EventKind::Push, static_cast<u8>(i), p.position});
        }
    }
}

void respawnFallenPlayers(World& world) {
    for (u32 i = 0; i < world.state.playerCount; ++i) {
        const mealkit::physics::BodyId id = world.playerBodies[i];
        if (world.physics.position(id).y < kKillPlaneY) {
            world.physics.setTransform(id, spawnPosition(world, i), {});
            world.physics.setLinearVelocity(id, {});
        }
    }
}

void sensePlayers(World& world) {
    for (u32 i = 0; i < world.state.playerCount; ++i) {
        Player& p = world.state.players[i];
        const mealkit::physics::BodyId id = world.playerBodies[i];
        p.position = world.physics.position(id);
        p.velocity = world.physics.linearVelocity(id);
        mealkit::physics::RayHit hit;
        p.grounded = world.physics.castRay(p.position, {0.0f, -(kPlayerHalfHeight + 0.12f), 0.0f}, hit, id) ? 1u : 0u;
    }
}

} // namespace sandbox
