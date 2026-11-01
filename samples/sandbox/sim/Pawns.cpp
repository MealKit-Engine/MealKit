// Player slots, controllers, Pawns: possession, movement, jumping, pushing, jump pads, respawn wait after falling, AI.
#include "sim/Systems.h"

#include "sim/Loadout.h"

#include "mealkit/core/DetMath.h"
#include "mealkit/debug/DebugDraw.h"

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
    mealkit::sim::MoveIntent move;
    bool push = false;
};

Intent intentFromInput(const GameInput& in) {
    Intent i;
    constexpr f32 kAxis = 127.0f; // const-ok: input quantization (i8 -127..127 → -1..1)
    i.move.wish = {static_cast<f32>(in.moveX) / kAxis, 0.0f, static_cast<f32>(in.moveZ) / kAxis};
    i.move.jump = (in.buttons & kButtonJump) != 0;
    i.push = (in.buttons & kButtonPush) != 0;
    return i;
}

// AI: occasionally picks a new nearby target point and walks there along a nav grid path (around obstacles, not off edges).
// Sometimes jumps or pushes when changing targets. Paths are searched in controller order, at most aiPathsPerTick per tick.
Intent thinkAI(World& world, Controller& c, const Pawn& pawn, u32& pathBudget) {
    const Tuning& t = world.tuning;
    const u32 tick = world.state.tick;
    Intent i;
    if (c.aiThink.ready(tick)) {
        mealkit::Pcg32& rng = world.state.aiRng;
        c.aiThink.start(tick, t.aiThink + rng.below(t.aiThink + 1));
        const f32 angle = rng.range(-mealkit::kDetPi, mealkit::kDetPi);
        const f32 distance = rng.range(t.aiTargetMin, t.aiTargetMax);
        const Vec3 target = pawn.position + Vec3{mealkit::detSin(angle), 0.0f, mealkit::detCos(angle)} * distance;
        c.aiTarget = {std::clamp(target.x, world.arenaMin.x, world.arenaMax.x), pawn.position.y - pawn.mover.halfHeight,
                      std::clamp(target.z, world.arenaMin.z, world.arenaMax.z)};
        c.aiRepath = true;
        c.aiPathCount = c.aiPathNext = 0;
        i.move.jump = rng.below(t.aiJumpOneIn) == 0;
        i.push = rng.below(t.aiPushOneIn) == 0;
    }
    const Vec3 feet{pawn.position.x, pawn.position.y - pawn.mover.halfHeight, pawn.position.z};
    if (c.aiRepath && pathBudget > 0 && !world.nav.empty()) {
        --pathBudget;
        c.aiRepath = false;
        u32 count = 0;
        mealkit::sim::findPath(world.nav, world.navSearch, feet, c.aiTarget, c.aiPath.data(), kAiPathPoints, count,
                               t.aiPathExpand);
        c.aiPathCount = count;
        c.aiPathNext = 0;
    }
    // Next waypoint (reached within half a meter), or the target itself without a path (waiting for a search, or no grid)
    constexpr f32 kReach2 = 0.25f; // const-ok: waypoint reach distance squared
    Vec3 goal = c.aiTarget;
    while (c.aiPathNext < c.aiPathCount) {
        const Vec3 w = c.aiPath[c.aiPathNext];
        const f32 dx = w.x - feet.x, dz = w.z - feet.z;
        if (dx * dx + dz * dz > kReach2) {
            goal = w;
            break;
        }
        if (++c.aiPathNext == c.aiPathCount) { // end of a path cut short at kAiPathPoints: search again from here
            const f32 tx = c.aiTarget.x - feet.x, tz = c.aiTarget.z - feet.z;
            if (c.aiPathCount == kAiPathPoints && tx * tx + tz * tz > 1.0f) c.aiRepath = true;
            goal = feet; // stand still for now
        }
    }
    if (t.navDebug) {
        Vec3 from = feet;
        for (u32 k = c.aiPathNext; k < c.aiPathCount; ++k) {
            mealkit::debug::line(from, c.aiPath[k], mealkit::debug::kGreen);
            from = c.aiPath[k];
        }
    }
    const Vec3 to{goal.x - pawn.position.x, 0.0f, goal.z - pawn.position.z};
    const f32 len2 = mealkit::lengthSq(to);
    if (len2 > (c.aiPathNext < c.aiPathCount ? 0.01f : 1.0f)) i.move.wish = to / std::sqrt(len2) * t.aiSpeedScale; // const-ok: stop distance² for waypoints / old 1 m² for the target
    return i;
}

// Nav grid debug drawing: walkable cells near each AI as short crosses (tuning navDebug)
void drawNav(const World& world) {
    const mealkit::sim::NavGrid& g = world.nav;
    if (!world.tuning.navDebug || g.empty()) return;
    constexpr mealkit::i32 kRings = 12; // const-ok: debug draw radius in cells
    const f32 half = g.cellSize * 0.3f; // const-ok: debug cross size
    for (auto [id, c] : world.state.controllers) {
        const Pawn* p = c.kind == ControllerKind::AI ? world.state.pawns.get(c.pawn) : nullptr;
        if (!p) continue;
        const mealkit::i32 cx = static_cast<mealkit::i32>((p->position.x - g.origin.x) / g.cellSize);
        const mealkit::i32 cz = static_cast<mealkit::i32>((p->position.z - g.origin.z) / g.cellSize);
        for (mealkit::i32 z = std::max(0, cz - kRings); z <= std::min(static_cast<mealkit::i32>(g.depth) - 1, cz + kRings); ++z)
            for (mealkit::i32 x = std::max(0, cx - kRings); x <= std::min(static_cast<mealkit::i32>(g.width) - 1, cx + kRings); ++x)
                for (u32 l = 0; l < g.layers; ++l) {
                    const u32 n = g.node(static_cast<u32>(x), static_cast<u32>(z), l);
                    if (!g.walkable(n)) continue;
                    const Vec3 q = g.position(n) + Vec3{0.0f, 0.03f, 0.0f}; // const-ok: lift debug lines off the floor
                    mealkit::debug::line(q - Vec3{half, 0.0f, 0.0f}, q + Vec3{half, 0.0f, 0.0f}, mealkit::debug::kBlue);
                    mealkit::debug::line(q - Vec3{0.0f, 0.0f, half}, q + Vec3{0.0f, 0.0f, half}, mealkit::debug::kBlue);
                }
    }
}

void movePawn(World& world, Pawn& p, const Intent& intent, u8 player) {
    const Tuning& t = world.tuning;
    const BodyKind* body = bodyOf(world, player);
    const f32 speedScale = body ? body->speedScale : 1.0f; // body kind (loadout)
    if (mealkit::sim::move(world.physics, p.mover, intent.move, t.move, speedScale, t.gravity, world.tickDt))
        world.events.push(makeEvent(EventKind::Jump, player, p.position, 0.0f));

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

// A capsule moved by the body kind's movement mode (engine mealkit/sim/Movement.h). Every mode pushes boxes away.
Id<Pawn> spawnPawn(World& world, const Vec3& position, const BodyKind* body) {
    mealkit::sim::MoverShape shape;
    shape.position = position;
    shape.halfHeight = world.tuning.pawnHalfHeight;
    shape.radius = world.tuning.pawnRadius;
    shape.mass = world.tuning.pawnMass * (body ? body->massScale : 1.0f);
    Pawn p;
    p.position = position;
    p.mover = mealkit::sim::createMover(world.physics, body ? body->movement : mealkit::sim::MoveMode::Capsule, shape,
                                        world.tuning.move);
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
        if (c.kind == ControllerKind::Player && slotOut(world, c.slot)) continue;     // out of lives: spectate until the next round
        const Vec3 at = spawnPosition(world, c.spawnIndex);
        const u8 player = c.kind == ControllerKind::Player ? static_cast<u8>(c.slot) : kNoPlayer;
        const BodyKind* body = bodyOf(world, player);
        const Id<Pawn> pawn = spawnPawn(world, at, body);
        possess(world, id, pawn);
        world.events.push(makeEvent(EventKind::Spawn, player, at, 0.0f));
    }
}

void controlPawns(World& world) {
    WorldState& s = world.state;
    const bool playing = matchPlaying(world); // during countdown and after the end, stand and wait (AI decisions stop too)
    u32 pathBudget = world.tuning.aiPathsPerTick; // per-tick search limit, used up in controller order
    drawNav(world);
    for (auto [id, c] : s.controllers) { // controller order = player slot order, then AI
        Pawn* p = s.pawns.get(c.pawn);
        if (!p) continue;
        if (c.kind == ControllerKind::Player)
            movePawn(world, *p, playing ? intentFromInput(world.inputs[c.slot]) : Intent{}, static_cast<u8>(c.slot));
        else if (c.kind == ControllerKind::AI) movePawn(world, *p, playing ? thinkAI(world, c, *p, pathBudget) : Intent{}, kNoPlayer);
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
            Vec3 v = mealkit::sim::moverVelocity(world.physics, p.mover);
            v.y = pad.power;
            mealkit::sim::setMoverVelocity(world.physics, p.mover, v);
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
        if (player != kNoPlayer && matchPlaying(world)) onPlayerFell(world, player); // score only during play
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
        mealkit::sim::senseMover(world.physics, p.mover, world.tuning.move, p.position, p.velocity);
    }
}

} // namespace sandbox
