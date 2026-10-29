// Player slots, controllers, Pawns: possession, movement, jumping, pushing, jump pads, respawn wait after falling, AI.
#include "sim/Systems.h"

#include "sim/Loadout.h"

#include "mealkit/core/DetMath.h"

#include <algorithm>
#include <cmath>

namespace sandbox {

const BodyKind* bodyOf(const World& world, u8 player) {
    if (player >= world.state.playerCount || world.bodyKinds.size() == 0) return nullptr;
    return &world.bodyKinds.rows[world.state.slots[player].body];
}

namespace {

// What a Pawn wants to do this tick (humans from input, AI from decisions)
struct Intent {
    Vec3 wish; // horizontal move direction, length 0..1
    bool jump = false;
    bool push = false;
};

Intent intentFromInput(const GameInput& in) {
    Intent i;
    constexpr f32 kAxis = 127.0f; // const-ok: input quantization (i8 -127..127 → -1..1)
    i.wish = {static_cast<f32>(in.moveX) / kAxis, 0.0f, static_cast<f32>(in.moveZ) / kAxis};
    i.jump = (in.buttons & kButtonJump) != 0;
    i.push = (in.buttons & kButtonPush) != 0;
    return i;
}

// AI: occasionally picks a new nearby target point and walks toward it. Sometimes jumps or pushes when changing targets.
Intent thinkAI(World& world, Controller& c, const Pawn& pawn) {
    const Tuning& t = world.tuning;
    const u32 tick = world.state.tick;
    Intent i;
    if (c.aiThink.ready(tick)) {
        mealkit::Pcg32& rng = world.state.aiRng;
        c.aiThink.start(tick, t.aiThink + rng.below(t.aiThink + 1));
        const f32 angle = rng.range(-mealkit::kDetPi, mealkit::kDetPi);
        const f32 distance = rng.range(t.aiTargetMin, t.aiTargetMax);
        const Vec3 target = pawn.position + Vec3{mealkit::detSin(angle), 0.0f, mealkit::detCos(angle)} * distance;
        c.aiTarget = {std::clamp(target.x, world.arenaMin.x, world.arenaMax.x), 0.0f,
                      std::clamp(target.z, world.arenaMin.z, world.arenaMax.z)};
        i.jump = rng.below(t.aiJumpOneIn) == 0;
        i.push = rng.below(t.aiPushOneIn) == 0;
    }
    const Vec3 to{c.aiTarget.x - pawn.position.x, 0.0f, c.aiTarget.z - pawn.position.z};
    const f32 len2 = mealkit::lengthSq(to);
    if (len2 > 1.0f) i.wish = to / std::sqrt(len2) * t.aiSpeedScale;
    return i;
}

void movePawn(World& world, Pawn& p, const Intent& intent, u8 player) {
    const Tuning& t = world.tuning;
    const BodyKind* body = bodyOf(world, player);
    const f32 speed = t.playerSpeed * (body ? body->speedScale : 1.0f); // body kind (loadout)
    Vec3 wish = intent.wish;
    const f32 wishLen2 = mealkit::lengthSq(wish);
    if (wishLen2 > 1.0f) wish = wish / std::sqrt(wishLen2);

    Vec3 v = world.physics.linearVelocity(p.body);
    const f32 accel = std::min(1.0f, (p.grounded ? t.groundAccel : t.airAccel) * world.tickDt);
    v.x += (wish.x * speed - v.x) * accel;
    v.z += (wish.z * speed - v.z) * accel;
    if (p.grounded && intent.jump && v.y < t.jumpMaxRise) {
        v.y = t.jumpSpeed;
        world.events.push(makeEvent(EventKind::Jump, player, p.position, 0.0f));
    }
    world.physics.setLinearVelocity(p.body, v);

    if (intent.push && cooldown::tryUse(p.push, world.state.tick, t.pushCooldown.value)) {
        pushProps(world, p.position);
        world.events.push(makeEvent(EventKind::Push, player, p.position, t.pushRadius));
    }
}

} // namespace

// Uses the scene's spawn points in turn, stepping aside if there are more than points. Scenes without spawn points line up sideways from the origin.
Vec3 spawnPosition(const World& world, u32 index) {
    const Tuning& t = world.tuning;
    if (world.spawns.empty()) return {static_cast<f32>(index) * t.spawnSpacing, t.pawnHalfHeight, 0.0f};
    const u32 n = static_cast<u32>(world.spawns.size());
    const f32 offset = static_cast<f32>(index / n) * t.spawnSpacing;
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

// One controller per player slot, then AI controllers (creation order = processing order)
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

// A capsule with locked rotation. Moved by setting velocity directly, and physically pushes boxes away.
Id<Pawn> spawnPawn(World& world, const Vec3& position, f32 massScale) {
    mealkit::physics::BodyDesc d;
    d.position = position;
    d.lockRotation = true;
    d.friction = 0.0f;
    d.restitution = 0.0f;
    d.linearDamping = 0.0f;
    d.mass = world.tuning.pawnMass * massScale;
    d.allowSleep = false;
    Pawn p;
    p.position = position;
    p.body = world.physics.createCapsule(world.tuning.pawnHalfHeight - world.tuning.pawnRadius, world.tuning.pawnRadius, d);
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

// Creates a new Pawn and possesses it for controllers whose respawn wait has ended (the first Pawns at match start are made this way too)
void respawnPawns(World& world) {
    WorldState& s = world.state;
    for (auto [id, c] : s.controllers) {
        if (s.pawns.alive(c.pawn) || !c.respawn.ready(s.tick)) continue;
        if (c.kind == ControllerKind::Player && !s.slots[c.slot].connected) continue; // disconnected players wait until they rejoin
        const Vec3 at = spawnPosition(world, c.spawnIndex);
        const u8 player = c.kind == ControllerKind::Player ? static_cast<u8>(c.slot) : kNoPlayer;
        const BodyKind* body = bodyOf(world, player);
        const Id<Pawn> pawn = spawnPawn(world, at, body ? body->massScale : 1.0f);
        possess(world, id, pawn);
        world.events.push(makeEvent(EventKind::Spawn, player, at, 0.0f));
    }
}

void controlPawns(World& world) {
    WorldState& s = world.state;
    const bool playing = matchPlaying(world); // during countdown and after the end, stand and wait (AI decisions stop too)
    for (auto [id, c] : s.controllers) { // controller order = player slot order, then AI
        Pawn* p = s.pawns.get(c.pawn);
        if (!p) continue;
        if (c.kind == ControllerKind::Player)
            movePawn(world, *p, playing ? intentFromInput(world.inputs[c.slot]) : Intent{}, static_cast<u8>(c.slot));
        else if (c.kind == ControllerKind::AI) movePawn(world, *p, playing ? thinkAI(world, c, *p) : Intent{}, kNoPlayer);
    }
}

void usePads(World& world) {
    WorldState& s = world.state;
    const u32 tick = s.tick;
    for (auto [padId, pad] : s.pads) {
        if (!pad.ready.ready(tick)) continue;
        const f32 top = pad.center.y + pad.halfExtents.y;
        for (auto [pawnId, p] : s.pawns) { // Pawn slot order
            const f32 feet = p.position.y - world.tuning.pawnHalfHeight;
            const Vec3 d = p.position - pad.center;
            const bool over = d.x <= pad.halfExtents.x && -d.x <= pad.halfExtents.x && d.z <= pad.halfExtents.z &&
                              -d.z <= pad.halfExtents.z;
            if (!over || feet < top - world.tuning.padBelow || feet > top + world.tuning.padAbove) continue;
            Vec3 v = world.physics.linearVelocity(p.body);
            v.y = pad.power;
            world.physics.setLinearVelocity(p.body, v);
            pad.ready.start(tick, pad.cooldownTicks);
            world.events.push(makeEvent(EventKind::Bounce, pawnPlayer(s, p), {pad.center.x, top, pad.center.z}, 0.0f));
            break;
        }
    }
}

// Pawns that fell below the match area floor are removed, and controllers wait to respawn (spectating meanwhile)
void removeFallenPawns(World& world) {
    WorldState& s = world.state;
    for (auto [pawnId, p] : s.pawns) {
        if (p.position.y >= world.arenaMin.y) continue;
        const u8 player = pawnPlayer(s, p);
        if (Controller* c = s.controllers.get(p.owner)) {
            c->respawn.start(s.tick, world.tuning.respawn);
            unpossess(world, p.owner);
        }
        if (player != kNoPlayer && matchPlaying(world)) ++s.slots[player].falls; // score only during play
        world.events.push(makeEvent(EventKind::Fall, player, p.position, 0.0f));
        s.pawns.destroy(pawnId); // the body is deleted at the end of the tick (World::step)
    }
}

// Someone joined mid-match: for a new slot, create a controller (Pawn on the next tick); for a rejoin, re-enable that slot.
// A disconnected player's Pawn is deleted (score/slot remain)
void setPresence(World& world, const mealkit::sim::Presence& presence) {
    WorldState& s = world.state;
    if (presence.player >= mealkit::net::kMaxPlayers) return;
    PlayerSlot& slot = s.slots[presence.player];
    if (!presence.connected) {
        slot.connected = false;
        if (Controller* c = s.controllers.get(slot.controller)) {
            if (Pawn* p = s.pawns.get(c->pawn)) {
                const Id<Pawn> pawn = c->pawn;
                world.events.push(makeEvent(EventKind::Fall, presence.player, p->position, 0.0f));
                unpossess(world, slot.controller);
                s.pawns.destroy(pawn);
            }
        }
        return;
    }
    slot.connected = true;
    if (presence.player < s.playerCount && s.controllers.alive(slot.controller)) return; // rejoined
    s.playerCount = std::max(s.playerCount, presence.player + 1u);
    Loadout l;
    slot.body = presence.readLoadout(l) && l.body < world.bodyKinds.size() ? l.body : 0u;
    if (s.controllers.count() >= s.controllers.capacity()) s.controllers.grow(s.controllers.capacity() + 1);
    s.pawns.grow(s.controllers.capacity() * 2); // same ratio as World::reset
    Controller c;
    c.kind = ControllerKind::Player;
    c.slot = presence.player;
    c.spawnIndex = s.controllers.count(); // a spawn spot that doesn't overlap existing controllers
    slot.controller = s.controllers.create(c);
}

void sensePawns(World& world) {
    for (auto [id, p] : world.state.pawns) {
        p.position = world.physics.position(p.body);
        p.velocity = world.physics.linearVelocity(p.body);
        mealkit::physics::RayHit hit;
        const f32 probe = world.tuning.pawnHalfHeight + world.tuning.groundProbe;
        p.grounded = world.physics.castRay(p.position, {0.0f, -probe, 0.0f}, hit, p.body) ? 1u : 0u;
    }
}

} // namespace sandbox
