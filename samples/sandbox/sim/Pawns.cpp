// 플레이어 슬롯·컨트롤러·Pawn: 빙의, 이동, 점프, 밀치기, 점프대, 떨어지면 리스폰 대기, AI.
#include "sim/Systems.h"

#include "mealkit/core/DetMath.h"

#include <algorithm>
#include <cmath>

namespace sandbox {
namespace {

// 씬에 스폰 지점이 없을 때 쓰는 기본 위치 (삼각함수 대신 고정 테이블)
constexpr Vec3 kDefaultSpawns[mealkit::net::kMaxPlayers] = {
    {-6, kPlayerHalfHeight, -6}, {6, kPlayerHalfHeight, 6}, {6, kPlayerHalfHeight, -6}, {-6, kPlayerHalfHeight, 6},
    {0, kPlayerHalfHeight, -9},  {0, kPlayerHalfHeight, 9}, {-9, kPlayerHalfHeight, 0}, {9, kPlayerHalfHeight, 0},
};
constexpr f32 kArenaHalf = 40.0f; // AI가 목표로 삼는 범위

// 이번 틱에 Pawn이 하려는 것 (사람은 입력에서, AI는 판단에서)
struct Intent {
    Vec3 wish; // 수평 이동 방향, 길이 0..1
    bool jump = false;
    bool push = false;
};

Intent intentFromInput(const GameInput& in) {
    Intent i;
    i.wish = {static_cast<f32>(in.moveX) / 127.0f, 0.0f, static_cast<f32>(in.moveZ) / 127.0f};
    i.jump = (in.buttons & kButtonJump) != 0;
    i.push = (in.buttons & kButtonPush) != 0;
    return i;
}

// AI: 가끔 근처의 새 목표 지점을 고르고 그쪽으로 걷는다. 목표를 바꿀 때 가끔 점프하거나 밀친다.
Intent thinkAI(World& world, Controller& c, const Pawn& pawn) {
    const Tuning& t = world.tuning;
    const u32 tick = world.state.tick;
    Intent i;
    if (c.aiThink.ready(tick)) {
        mealkit::Pcg32& rng = world.state.aiRng;
        c.aiThink.start(tick, t.aiThink + rng.below(t.aiThink + 1));
        const f32 angle = rng.range(-mealkit::kDetPi, mealkit::kDetPi);
        const f32 distance = rng.range(4.0f, 14.0f);
        const Vec3 target = pawn.position + Vec3{mealkit::detSin(angle), 0.0f, mealkit::detCos(angle)} * distance;
        c.aiTarget = {std::clamp(target.x, -kArenaHalf, kArenaHalf), 0.0f,
                      std::clamp(target.z, -kArenaHalf, kArenaHalf)};
        i.jump = rng.below(4) == 0;
        i.push = rng.below(3) == 0;
    }
    const Vec3 to{c.aiTarget.x - pawn.position.x, 0.0f, c.aiTarget.z - pawn.position.z};
    const f32 len2 = mealkit::lengthSq(to);
    if (len2 > 1.0f) i.wish = to / std::sqrt(len2) * t.aiSpeedScale;
    return i;
}

void movePawn(World& world, Pawn& p, const Intent& intent, u8 player) {
    const Tuning& t = world.tuning;
    Vec3 wish = intent.wish;
    const f32 wishLen2 = mealkit::lengthSq(wish);
    if (wishLen2 > 1.0f) wish = wish / std::sqrt(wishLen2);

    Vec3 v = world.physics.linearVelocity(p.body);
    const f32 accel = std::min(1.0f, (p.grounded ? t.groundAccel : t.airAccel) * kTickDt);
    v.x += (wish.x * t.playerSpeed - v.x) * accel;
    v.z += (wish.z * t.playerSpeed - v.z) * accel;
    if (p.grounded && intent.jump && v.y < 1.0f) v.y = t.jumpSpeed;
    world.physics.setLinearVelocity(p.body, v);

    if (intent.push && p.pushReady.ready(world.state.tick)) {
        p.pushReady.start(world.state.tick, t.pushCooldown);
        pushProps(world, p.position);
        world.events.push({EventKind::Push, player, p.position, t.pushRadius});
    }
}

} // namespace

Vec3 spawnPosition(const World& world, u32 index) {
    if (world.spawns.empty()) return kDefaultSpawns[index % mealkit::net::kMaxPlayers];
    const u32 n = static_cast<u32>(world.spawns.size());
    const f32 offset = static_cast<f32>(index / n) * 1.2f; // 스폰 지점보다 많으면 옆으로 비켜 선다
    return world.spawns[index % n] + Vec3{offset, 0.0f, 0.0f};
}

const Pawn* playerPawn(const WorldState& state, u32 slot) {
    if (slot >= state.playerCount) return nullptr;
    const Controller* c = state.controllers.get(state.slots[slot].controller);
    return c ? state.pawns.get(c->pawn) : nullptr;
}

u8 pawnPlayer(const WorldState& state, const Pawn& pawn) {
    const Controller* c = state.controllers.get(pawn.owner);
    return c && c->kind == ControllerKind::Player ? static_cast<u8>(c->slot) : kNoPlayer;
}

// 플레이어 슬롯마다 컨트롤러 하나, 그다음 AI 컨트롤러 (만드는 순서 = 처리 순서)
void createControllers(World& world) {
    WorldState& s = world.state;
    for (u32 slot = 0; slot < s.playerCount; ++slot) {
        Controller c;
        c.kind = ControllerKind::Player;
        c.slot = slot;
        c.spawnIndex = slot;
        s.slots[slot].connected = true;
        s.slots[slot].controller = s.controllers.create(c);
    }
    for (u32 i = 0; i < world.tuning.aiPawns; ++i) {
        Controller c;
        c.kind = ControllerKind::AI;
        c.spawnIndex = s.playerCount + i;
        s.controllers.create(c);
    }
}

// 회전이 잠긴 캡슐. 속도를 직접 정해 움직이고, 물리적으로 상자를 밀어낸다.
Id<Pawn> spawnPawn(World& world, const Vec3& position) {
    mealkit::physics::BodyDesc d;
    d.position = position;
    d.lockRotation = true;
    d.friction = 0.0f;
    d.restitution = 0.0f;
    d.linearDamping = 0.0f;
    d.mass = 80.0f;
    d.allowSleep = false;
    Pawn p;
    p.position = position;
    p.body = world.physics.createCapsule(kPlayerHalfHeight - kPlayerRadius, kPlayerRadius, d);
    return world.state.pawns.create(p);
}

void possess(World& world, Id<Controller> controller, Id<Pawn> pawn) {
    unpossess(world, controller);
    Controller* c = world.state.controllers.get(controller);
    Pawn* p = world.state.pawns.get(pawn);
    if (!c || !p) return;
    if (Controller* previous = world.state.controllers.get(p->owner)) previous->pawn = {};
    c->pawn = pawn;
    p->owner = controller;
}

void unpossess(World& world, Id<Controller> controller) {
    Controller* c = world.state.controllers.get(controller);
    if (!c) return;
    if (Pawn* p = world.state.pawns.get(c->pawn)) p->owner = {};
    c->pawn = {};
}

// 리스폰 대기가 끝난 컨트롤러에 새 Pawn을 만들어 빙의시킨다 (매치 시작 때도 이것으로 처음 Pawn을 만든다)
void respawnPawns(World& world) {
    WorldState& s = world.state;
    for (auto [id, c] : s.controllers) {
        if (s.pawns.alive(c.pawn) || !c.respawn.ready(s.tick)) continue;
        const Vec3 at = spawnPosition(world, c.spawnIndex);
        const Id<Pawn> pawn = spawnPawn(world, at);
        possess(world, id, pawn);
        world.events.push(
            {EventKind::Spawn, c.kind == ControllerKind::Player ? static_cast<u8>(c.slot) : kNoPlayer, at, 0.0f});
    }
}

void controlPawns(World& world) {
    WorldState& s = world.state;
    for (auto [id, c] : s.controllers) { // 컨트롤러 순서 = 플레이어 슬롯 순서, 그다음 AI
        Pawn* p = s.pawns.get(c.pawn);
        if (!p) continue;
        if (c.kind == ControllerKind::Player)
            movePawn(world, *p, intentFromInput(world.inputs[c.slot]), static_cast<u8>(c.slot));
        else if (c.kind == ControllerKind::AI) movePawn(world, *p, thinkAI(world, c, *p), kNoPlayer);
    }
}

void usePads(World& world) {
    WorldState& s = world.state;
    const u32 tick = s.tick;
    for (auto [padId, pad] : s.pads) {
        if (!pad.ready.ready(tick)) continue;
        const f32 top = pad.center.y + pad.halfExtents.y;
        for (auto [pawnId, p] : s.pawns) { // Pawn 슬롯 순서
            const f32 feet = p.position.y - kPlayerHalfHeight;
            const Vec3 d = p.position - pad.center;
            const bool over = d.x <= pad.halfExtents.x && -d.x <= pad.halfExtents.x && d.z <= pad.halfExtents.z &&
                              -d.z <= pad.halfExtents.z;
            if (!over || feet < top - 0.3f || feet > top + 0.4f) continue;
            Vec3 v = world.physics.linearVelocity(p.body);
            v.y = pad.power;
            world.physics.setLinearVelocity(p.body, v);
            pad.ready.start(tick, pad.cooldownTicks);
            world.events.push({EventKind::Bounce, pawnPlayer(s, p), {pad.center.x, top, pad.center.z}, 0.0f});
            break;
        }
    }
}

// 떨어진 Pawn은 없애고, 컨트롤러는 리스폰을 기다린다 (그동안 관전)
void removeFallenPawns(World& world) {
    WorldState& s = world.state;
    for (auto [pawnId, p] : s.pawns) {
        if (p.position.y >= kKillPlaneY) continue;
        const u8 player = pawnPlayer(s, p);
        if (Controller* c = s.controllers.get(p.owner)) {
            c->respawn.start(s.tick, world.tuning.respawn);
            unpossess(world, p.owner);
        }
        if (player != kNoPlayer) ++s.slots[player].falls;
        world.events.push({EventKind::Fall, player, p.position, 0.0f});
        s.pawns.destroy(pawnId); // 바디는 틱 끝에 지운다 (World::step)
    }
}

void sensePawns(World& world) {
    for (auto [id, p] : world.state.pawns) {
        p.position = world.physics.position(p.body);
        p.velocity = world.physics.linearVelocity(p.body);
        mealkit::physics::RayHit hit;
        p.grounded =
            world.physics.castRay(p.position, {0.0f, -(kPlayerHalfHeight + 0.12f), 0.0f}, hit, p.body) ? 1u : 0u;
    }
}

} // namespace sandbox
