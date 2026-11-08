// Player characters: spawn, movement, respawn after falling, players joining and leaving mid-match.
#include "sim/Systems.h"

#include <algorithm>

namespace sandbox {
namespace {

// The scene's spawn points in turn, stepping aside if there are more players than points. Without spawn points: a row from the origin
Vec3 spawnPosition(const World& world, u32 index) {
    const Tuning& t = world.tuning;
    if (world.spawns.empty()) return {static_cast<f32>(index) * t.spawnSpacing, t.pawnHalfHeight, 0.0f};
    const u32 n = static_cast<u32>(world.spawns.size());
    const f32 offset = static_cast<f32>(index / n) * t.spawnSpacing;
    return world.spawns[index % n] + Vec3{offset, 0.0f, 0.0f};
}

// A capsule moved by the engine's movement mode (mealkit/sim/Movement.h)
Id<Pawn> spawnPawn(World& world, u32 player, const Vec3& position) {
    Pawn p;
    p.player = player;
    p.position = position;
    const Id<Pawn> id = world.state.pawns.create(p);
    mealkit::sim::MoverShape shape;
    shape.position = position;
    shape.halfHeight = world.tuning.pawnHalfHeight;
    shape.radius = world.tuning.pawnRadius;
    shape.mass = world.tuning.pawnMass;
    shape.group = id.slot + 1;
    world.state.pawns.get(id)->mover =
        mealkit::sim::createMover(world.physics, mealkit::sim::MoveMode::Capsule, shape, world.tuning.move);
    return id;
}

mealkit::sim::MoveIntent intentFromInput(const GameInput& in) {
    constexpr f32 kAxis = 127.0f; // const-ok: input quantization (i8 -127..127 → -1..1)
    mealkit::sim::MoveIntent i;
    i.wish = {static_cast<f32>(in.moveX) / kAxis, 0.0f, static_cast<f32>(in.moveZ) / kAxis};
    i.jump = (in.buttons & kButtonJump) != 0;
    return i;
}

} // namespace

const Pawn* playerPawn(const WorldState& state, u32 slot) {
    return slot < state.playerCount ? state.pawns.get(state.slots[slot].pawn) : nullptr;
}

void respawnPawns(World& world) {
    WorldState& s = world.state;
    for (u32 slot = 0; slot < s.playerCount; ++slot) { // player slot order
        PlayerSlot& p = s.slots[slot];
        if (!p.connected || s.pawns.alive(p.pawn) || !p.respawn.ready(s.tick)) continue;
        const Vec3 at = spawnPosition(world, slot);
        p.pawn = spawnPawn(world, slot, at);
        world.events.push(makeEvent(EventKind::Spawn, static_cast<u8>(slot), at, 0.0f));
    }
}

void movePawns(World& world) {
    const bool playing = matchPlaying(world); // during countdown and after the end, stand and wait
    for (auto [id, p] : world.state.pawns) {  // Pawn slot order
        const mealkit::sim::MoveIntent intent = playing ? intentFromInput(world.inputs[p.player]) : mealkit::sim::MoveIntent{};
        if (mealkit::sim::move(world.physics, p.mover, intent, world.tuning.move, 1.0f, world.tuning.gravity, world.tickDt))
            world.events.push(makeEvent(EventKind::Jump, static_cast<u8>(p.player), p.position, 0.0f));
    }
}

void removeFallenPawns(World& world) {
    WorldState& s = world.state;
    for (auto [id, p] : s.pawns) {
        if (p.position.y >= world.tuning.fallHeight) continue;
        PlayerSlot& slot = s.slots[p.player];
        slot.pawn = {};
        slot.respawn.start(s.tick, world.tuning.respawn);
        world.events.push(makeEvent(EventKind::Fall, static_cast<u8>(p.player), p.position, 0.0f));
        s.pawns.destroy(id); // the body is deleted at the end of the tick (World::step)
    }
}

void sensePawns(World& world) {
    for (auto [id, p] : world.state.pawns) mealkit::sim::senseMover(world.physics, p.mover, world.tuning.move, p.position, p.velocity);
}

// Someone joined or left mid-match. A leaving player's character is removed (the slot and score remain); a joining player gets one next tick
void setPresence(World& world, const mealkit::sim::Presence& presence) {
    WorldState& s = world.state;
    if (presence.player >= mealkit::session::kMaxPlayers) return;
    PlayerSlot& slot = s.slots[presence.player];
    slot.connected = presence.connected;
    if (!presence.connected) {
        if (s.pawns.alive(slot.pawn)) s.pawns.destroy(slot.pawn);
        slot.pawn = {};
        return;
    }
    s.playerCount = std::max(s.playerCount, presence.player + 1u);
    slot.respawn = {};
}

} // namespace sandbox
