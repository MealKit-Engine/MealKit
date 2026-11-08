// Heroes and chasers: possession, movement, shoves, catching, reviving, falling off, chaser AI on the nav grid.
#include "sim/Systems.h"

#include "sim/Loadout.h"

#include "mealkit/core/DetMath.h"
#include "mealkit/debug/DebugDraw.h"

#include <algorithm>
#include <cmath>

namespace coop {
namespace {

using mealkit::sim::MoveIntent;

struct Intent {
    MoveIntent move;
    bool shove = false;
};

f32 flatDistance2(const Vec3& a, const Vec3& b) {
    const f32 dx = a.x - b.x, dz = a.z - b.z;
    return dx * dx + dz * dz;
}

Vec3 feetOf(const Pawn& p) { return {p.position.x, p.position.y - p.mover.halfHeight, p.position.z}; }

Intent intentFromInput(const GameInput& in) {
    Intent i;
    constexpr f32 kAxis = 127.0f; // const-ok: input quantization (i8 -127..127 → -1..1)
    i.move.wish = {static_cast<f32>(in.moveX) / kAxis, 0.0f, static_cast<f32>(in.moveZ) / kAxis};
    i.move.jump = (in.buttons & kButtonJump) != 0;
    i.shove = (in.buttons & kButtonPush) != 0;
    return i;
}

// The nearest hero that is up, within sight (Pawn slot order on ties)
Id<Pawn> nearestPrey(const World& world, const Vec3& from) {
    Id<Pawn> best{};
    f32 bestD2 = world.tuning.sightRadius * world.tuning.sightRadius;
    for (auto [id, p] : world.state.pawns) {
        if (p.kind != PawnKind::Hero || p.downed) continue;
        const f32 d2 = flatDistance2(p.position, from);
        if (d2 < bestD2) bestD2 = d2, best = id;
    }
    return best;
}

// Chaser AI: every aiThink ticks picks the nearest hero in sight (or a wander point) and searches a nav grid path to it.
// Paths are searched in controller order, at most aiPathsPerTick per tick.
MoveIntent thinkChaser(World& world, Controller& c, const Pawn& pawn, u32& pathBudget) {
    const Tuning& t = world.tuning;
    const u32 tick = world.state.tick;
    const Vec3 feet = feetOf(pawn);
    MoveIntent move;
    if (!pawn.stunned.ready(tick)) return move;
    if (c.aiThink.ready(tick)) {
        mealkit::Pcg32& rng = world.state.aiRng;
        c.aiThink.start(tick, t.aiThink + rng.below(t.aiThink / 2 + 1));
        c.prey = nearestPrey(world, pawn.position);
        if (!world.state.pawns.alive(c.prey)) { // wander near where it is
            const f32 angle = rng.range(-mealkit::kDetPi, mealkit::kDetPi);
            const Vec3 to = feet + Vec3{mealkit::detSin(angle), 0.0f, mealkit::detCos(angle)} * t.wanderRadius;
            c.aiPath[0] = {std::clamp(to.x, world.arenaMin.x, world.arenaMax.x), feet.y,
                           std::clamp(to.z, world.arenaMin.z, world.arenaMax.z)};
            c.aiPathCount = 1;
            c.aiPathNext = 0;
        }
        c.aiRepath = world.state.pawns.alive(c.prey);
    }
    if (c.aiRepath && pathBudget > 0 && !world.nav.empty()) {
        --pathBudget;
        c.aiRepath = false;
        const Pawn* prey = world.state.pawns.get(c.prey);
        u32 count = 0;
        if (prey) mealkit::sim::findPath(world.nav, world.navSearch, feet, feetOf(*prey), c.aiPath.data(), kAiPathPoints, count, t.aiPathExpand);
        c.aiPathCount = count;
        c.aiPathNext = 0;
    }
    // Close to the prey: straight at it. Else the next waypoint (reached within half a meter)
    Vec3 goal = feet;
    const Pawn* prey = world.state.pawns.get(c.prey);
    constexpr f32 kDirect2 = 9.0f; // const-ok: within 3 m chase directly (the path ends where the prey was)
    if (prey && !prey->downed && flatDistance2(prey->position, pawn.position) < kDirect2) {
        goal = prey->position;
    } else {
        constexpr f32 kReach2 = 0.25f; // const-ok: waypoint reach distance squared
        while (c.aiPathNext < c.aiPathCount) {
            const Vec3 w = c.aiPath[c.aiPathNext];
            if (flatDistance2(w, feet) > kReach2) {
                goal = w;
                break;
            }
            ++c.aiPathNext;
        }
    }
    if (t.navDebug) {
        Vec3 from = feet;
        for (u32 k = c.aiPathNext; k < c.aiPathCount; ++k) {
            mealkit::debug::line(from, c.aiPath[k], mealkit::debug::kRed);
            from = c.aiPath[k];
        }
    }
    const Vec3 to{goal.x - pawn.position.x, 0.0f, goal.z - pawn.position.z};
    const f32 len2 = mealkit::lengthSq(to);
    if (len2 > 0.01f) move.wish = to / std::sqrt(len2) * t.chaserSpeedScale; // const-ok: stop distance²
    return move;
}

// Shove: chasers within range are stunned and thrown back (heroes aren't shoved: it's co-op)
void shove(World& world, const Pawn& hero, u8 player) {
    const Tuning& t = world.tuning;
    for (auto [id, other] : world.state.pawns) { // Pawn slot order
        if (other.kind != PawnKind::Chaser) continue;
        const Vec3 d = other.position - hero.position;
        const f32 dist2 = flatDistance2(other.position, hero.position);
        if (dist2 >= t.shoveRadius * t.shoveRadius) continue;
        const f32 dist = std::sqrt(dist2);
        const Vec3 dir = dist > 1e-3f ? Vec3{d.x / dist, 0.0f, d.z / dist} : Vec3{0.0f, 0.0f, 1.0f}; // const-ok: overlapping
        mealkit::sim::setMoverVelocity(world.physics, other.mover, dir * t.shoveSpeed + Vec3{0.0f, t.shoveLift, 0.0f});
        other.stunned.start(world.state.tick, t.shoveStun.value);
        world.events.push(makeEvent(EventKind::Stunned, player, other.position, 0.0f));
    }
    world.events.push(makeEvent(EventKind::Push, player, hero.position, t.shoveRadius));
}

Id<Pawn> spawnPawn(World& world, PawnKind kind, const Vec3& position, const BodyKind* body) {
    const Tuning& t = world.tuning;
    Pawn p;
    p.kind = kind;
    p.position = position;
    const Id<Pawn> id = world.state.pawns.create(p);
    mealkit::sim::MoverShape shape;
    shape.position = position;
    shape.group = id.slot + 1; // ragdoll parts: unique among living Pawns
    mealkit::sim::MoveMode mode = mealkit::sim::MoveMode::Kinematic;
    if (kind == PawnKind::Hero) {
        shape.halfHeight = t.heroHalfHeight;
        shape.radius = t.heroRadius;
        shape.mass = t.heroMass * (body ? body->massScale : 1.0f);
        mode = body ? body->movement : mealkit::sim::MoveMode::Ragdoll;
    } else {
        shape.halfHeight = t.chaserHalfHeight;
        shape.radius = t.chaserRadius;
        shape.mass = t.chaserMass;
    }
    world.state.pawns.get(id)->mover =
        mealkit::sim::createMover(world.physics, mode, shape, kind == PawnKind::Hero ? t.move : t.chaserMove);
    return id;
}

Vec3 heroSpawn(const World& world, u32 index) {
    const Tuning& t = world.tuning;
    if (world.spawns.empty()) return {static_cast<f32>(index) * t.spawnSpacing, t.heroHalfHeight, 0.0f};
    const u32 n = static_cast<u32>(world.spawns.size());
    return world.spawns[index % n] + Vec3{static_cast<f32>(index / n) * t.spawnSpacing, 0.0f, 0.0f};
}

Vec3 chaserSpawn(const World& world, u32 index) {
    u32 first = 0;
    for (const World::ChaserSpawn& s : world.chaserSpawns) {
        if (index < first + s.count) {
            const f32 side = static_cast<f32>(index - first) * world.tuning.spawnSpacing;
            return s.position + Vec3{side, 0.0f, 0.0f};
        }
        first += s.count;
    }
    return {};
}

} // namespace

const BodyKind* bodyOf(const World& world, u8 player) {
    if (player >= world.state.playerCount || world.bodyKinds.size() == 0) return nullptr;
    return &world.bodyKinds.rows[world.state.slots[player].body];
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

f32 reviveProgress(const World& world, const Pawn& pawn) {
    const u32 need = world.tuning.reviveTime.value;
    return need == 0 ? 1.0f : std::min(1.0f, static_cast<f32>(pawn.revive) / static_cast<f32>(need));
}

// One controller per player slot, then chasers (creation order = processing order)
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
    u32 index = 0;
    for (const World::ChaserSpawn& spawn : world.chaserSpawns) {
        for (u32 i = 0; i < spawn.count; ++i) {
            Controller c;
            c.kind = ControllerKind::Chaser;
            c.spawnIndex = index++;
            s.controllers.create(c);
        }
    }
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

void respawnPawns(World& world) {
    WorldState& s = world.state;
    for (auto [id, c] : s.controllers) {
        if (s.pawns.alive(c.pawn) || !c.respawn.ready(s.tick)) continue;
        if (c.kind == ControllerKind::Player && !s.slots[c.slot].connected) continue; // disconnected players wait until they rejoin
        const bool hero = c.kind == ControllerKind::Player;
        const u8 player = hero ? static_cast<u8>(c.slot) : kNoPlayer;
        const Vec3 at = hero ? heroSpawn(world, c.spawnIndex) : chaserSpawn(world, c.spawnIndex);
        const Id<Pawn> pawn = spawnPawn(world, hero ? PawnKind::Hero : PawnKind::Chaser, at, bodyOf(world, player));
        possess(world, id, pawn);
        world.events.push(makeEvent(EventKind::Spawn, player, at, 0.0f));
    }
}

void controlPawns(World& world) {
    WorldState& s = world.state;
    const Tuning& t = world.tuning;
    const bool playing = matchPlaying(world); // countdown and results: stand and wait
    u32 pathBudget = t.aiPathsPerTick;
    for (auto [id, c] : s.controllers) { // controller order = player slots, then chasers
        Pawn* p = s.pawns.get(c.pawn);
        if (!p) continue;
        if (c.kind == ControllerKind::Player) {
            const u8 player = static_cast<u8>(c.slot);
            Intent intent = playing && !p->downed ? intentFromInput(world.inputs[c.slot]) : Intent{};
            if (p->downed) mealkit::sim::knockRagdoll(world.physics, p->mover, 2, {}); // stays limp until revived
            const BodyKind* body = bodyOf(world, player);
            if (mealkit::sim::move(world.physics, p->mover, intent.move, t.move, body ? body->speedScale : 1.0f, t.gravity, world.tickDt))
                world.events.push(makeEvent(EventKind::Jump, player, p->position, 0.0f));
            if (intent.shove && p->shove.ready(s.tick)) {
                p->shove.start(s.tick, t.shoveCooldown.value);
                shove(world, *p, player);
            }
        } else if (c.kind == ControllerKind::Chaser) {
            const MoveIntent move = playing ? thinkChaser(world, c, *p, pathBudget) : MoveIntent{};
            mealkit::sim::move(world.physics, p->mover, move, t.chaserMove, 1.0f, t.gravity, world.tickDt);
        }
    }
}

// A chaser that's not resting and touches a hero who is up knocks them down, then rests
void catchHeroes(World& world) {
    if (!matchPlaying(world)) return;
    WorldState& s = world.state;
    const Tuning& t = world.tuning;
    for (auto [chaserId, chaser] : s.pawns) {
        if (chaser.kind != PawnKind::Chaser || !chaser.stunned.ready(s.tick)) continue;
        for (auto [heroId, hero] : s.pawns) {
            if (hero.kind != PawnKind::Hero || hero.downed) continue;
            if (flatDistance2(hero.position, chaser.position) > t.catchRadius * t.catchRadius) continue;
            if (std::fabs(hero.position.y - chaser.position.y) > hero.mover.halfHeight + chaser.mover.halfHeight) continue;
            hero.downed = true;
            hero.revive = 0;
            const Vec3 d = hero.position - chaser.position;
            const f32 len = std::sqrt(d.x * d.x + d.z * d.z);
            const Vec3 dir = len > 1e-3f ? Vec3{d.x / len, 0.0f, d.z / len} : Vec3{0.0f, 0.0f, 1.0f}; // const-ok: overlapping
            mealkit::sim::knockRagdoll(world.physics, hero.mover, 2, dir * t.catchKnock + Vec3{0.0f, t.catchKnock * 0.5f, 0.0f});
            chaser.stunned.start(s.tick, t.catchRest.value);
            const u8 player = pawnPlayer(s, hero);
            if (player != kNoPlayer) ++s.slots[player].downs;
            world.events.push(makeEvent(EventKind::Caught, player, hero.position, 0.0f));
            break;
        }
    }
}

// Downed heroes with a teammate (up) next to them make progress; the progress drops back to 0 when nobody is near
void reviveHeroes(World& world) {
    WorldState& s = world.state;
    const Tuning& t = world.tuning;
    for (auto [downedId, hero] : s.pawns) {
        if (hero.kind != PawnKind::Hero || !hero.downed) continue;
        u8 helper = kNoPlayer;
        for (auto [otherId, other] : s.pawns) {
            if (other.kind != PawnKind::Hero || other.downed || otherId == downedId) continue;
            if (flatDistance2(other.position, hero.position) > t.reviveRadius * t.reviveRadius) continue;
            helper = pawnPlayer(s, other);
            break;
        }
        if (helper == kNoPlayer && hero.revive > 0) hero.revive = 0;
        if (helper == kNoPlayer) continue;
        if (++hero.revive < t.reviveTime.value) continue;
        hero.downed = false;
        hero.revive = 0;
        if (helper < s.playerCount) ++s.slots[helper].revives;
        world.events.push(makeEvent(EventKind::Revive, pawnPlayer(s, hero), hero.position, 0.0f));
    }
}

// Pawns below the match area are removed; their controllers come back at a spawn point after the respawn wait (a downed hero comes back up)
void removeFallenPawns(World& world) {
    WorldState& s = world.state;
    for (auto [pawnId, p] : s.pawns) {
        if (p.position.y >= world.arenaMin.y) continue;
        const u8 player = pawnPlayer(s, p);
        if (Controller* c = s.controllers.get(p.owner)) {
            c->respawn.start(s.tick, world.tuning.respawn);
            unpossess(world, p.owner);
        }
        world.events.push(makeEvent(EventKind::Fall, player, p.position, 0.0f));
        s.pawns.destroy(pawnId); // the body is deleted at the end of the tick (World::step)
    }
}

// Someone joined mid-match: a new slot gets a controller (hero on the next tick); a rejoin re-enables that slot.
// A disconnected player's hero is removed
void setPresence(World& world, const mealkit::sim::Presence& presence) {
    WorldState& s = world.state;
    if (presence.player >= mealkit::session::kMaxPlayers) return;
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
    c.spawnIndex = presence.player;
    slot.controller = s.controllers.create(c);
}

void sensePawns(World& world) {
    for (auto [id, p] : world.state.pawns) {
        const mealkit::sim::MoveTuning& t = p.kind == PawnKind::Hero ? world.tuning.move : world.tuning.chaserMove;
        mealkit::sim::senseMover(world.physics, p.mover, t, p.position, p.velocity);
    }
}

} // namespace coop
