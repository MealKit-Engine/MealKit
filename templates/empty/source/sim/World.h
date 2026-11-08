#pragma once

#include "mealkit/asset/ModelCache.h"
#include "mealkit/core/Math.h"
#include "mealkit/session/Session.h"
#include "mealkit/physics/PhysicsWorld.h"
#include "mealkit/scene/Scene.h"
#include "mealkit/sim/MatchFlow.h"
#include "mealkit/sim/Movement.h"
#include "mealkit/sim/Pool.h"
#include "mealkit/sim/TickEvents.h"
#include "mealkit/sim/TickTimer.h"
#include "sim/Objects.h"
#include "sim/Tuning.h"

#include <vector>

// Deterministic simulation (physics: Jolt). Starts as one character per player walking on the scene's solid objects.
// sim/ folder rules (checked by SimCheck at build time): no CRT trig, real time, rand, renderer/platform headers, or mutable global state.
// State is declared with MEALKIT_FIELD: the hash (desync detection), snapshots (rewind/shadow simulation), and the editor [State] panel use it as is.
// Objects created and destroyed mid-match use Pool<T>, "after N ticks" uses TickTimer, and things to tell the view use world.events.
namespace sandbox {

using mealkit::f32;
using mealkit::i8;
using mealkit::u32;
using mealkit::u64;
using mealkit::u8;
using mealkit::Vec3;
using mealkit::sim::Id;
using mealkit::sim::TickTimer;

inline constexpr u8 kButtonJump = 1;

// Player input sent over the network (8 bytes). The client turns the camera-relative stick into a quantized world direction.
struct GameInput {
    i8 moveX = 0; // -127..127
    i8 moveZ = 0;
    u8 buttons = 0;
    u8 reserved[5] = {};
};
static_assert(sizeof(GameInput) == mealkit::session::kInputBytes);

// A character in the world. Disappears when it falls, and a new one appears on respawn.
MEALKIT_STRUCT(Pawn) {
    MEALKIT_FIELD(u32, player, 0, "플레이어 슬롯");
    MEALKIT_FIELD(Vec3, position, {}, "위치");
    MEALKIT_FIELD(Vec3, velocity, {}, "속도");
    MEALKIT_FIELD(mealkit::sim::Mover, mover, {}, "이동 (모드, 물리 바디, 땅에 닿음)");
};

// A connected person. Persists through the match.
MEALKIT_STRUCT(PlayerSlot) {
    MEALKIT_FIELD(bool, connected, false, "접속");
    MEALKIT_FIELD(u32, score, 0, "점수");
    MEALKIT_FIELD(Id<Pawn>, pawn, {}, "캐릭터 (없으면 리스폰 대기)");
    MEALKIT_FIELD(TickTimer, respawn, {}, "리스폰");
};

// Simulation → view notifications (sound/effects). sim/ only writes them. The feedback table (assets/feedback.table) chooses feedback by kind name.
MEALKIT_ENUM(EventKind, u8, Spawn, Jump, Fall, Countdown, MatchStart, MatchEnd);
inline constexpr u8 kNoPlayer = 0xFF;

MEALKIT_STRUCT(GameEvent) {
    MEALKIT_FIELD(EventKind, kind, EventKind::Spawn, "종류");
    MEALKIT_FIELD(u8, player, kNoPlayer, "플레이어 슬롯");
    MEALKIT_FIELD(Vec3, position, {}, "위치");
    MEALKIT_FIELD(f32, value, 0.0f, "카운트다운: 남은 초");
};

inline GameEvent makeEvent(EventKind kind, u8 player, Vec3 position, f32 value) {
    GameEvent e;
    e.kind = kind;
    e.player = player;
    e.position = position;
    e.value = value;
    return e;
}
inline constexpr u32 kMaxEventsPerTick = 32;

// Match phase (engine mealkit/sim/MatchFlow.h, driven by sim/Match.cpp). The screen flow opens the results screen at Results
using MatchPhase = mealkit::sim::FlowPhase;

MEALKIT_STRUCT(WorldState) {
    MEALKIT_FIELD(u32, tick, 0, "틱");
    MEALKIT_FIELD(mealkit::sim::MatchFlow, flow, {}, "매치 흐름 (단계, 라운드, 승자)");
    MEALKIT_FIELD(u32, playerCount, 0, "플레이어 수");
    MEALKIT_ARRAY(PlayerSlot, mealkit::session::kMaxPlayers, slots, "플레이어 슬롯");
    MEALKIT_FIELD(mealkit::sim::Pool<Pawn>, pawns, {}, "캐릭터");
};

// The whole simulation. sim/ system functions (Systems.h) take only this (World&).
struct World {
    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // physicsThreads = 0 matches the core count (results are independent of thread count)
    void reset(u64 seed, u32 playerCount, const Tuning& tuning, const mealkit::scene::Scene& scene,
               mealkit::asset::ModelCache& models, u32 tickRate, u32 physicsThreads = 0);
    // Changes tuning mid-match (editor solo play only)
    void setTuning(const Tuning& value);
    void step(const mealkit::session::TickInputs& inputs);
    u64 hash() const;
    u32 activeBodies() const { return physics.activeBodyCount(); }

    // Data decided at match start
    u32 tickRate = 30;
    f32 tickDt = 1.0f / 30.0f; // const-ok: default (reset sets it from the project tick rate)
    std::vector<Vec3> spawns;  // the scene's spawn points
    Tuning tuning;
    mealkit::sim::MatchRules rules; // from tuning (sim/Match.cpp matchRules)

    // State
    WorldState state;
    GameInput inputs[mealkit::session::kMaxPlayers] = {}; // this tick's inputs (filled at the start of step)
    mealkit::physics::PhysicsWorld physics;

    // This tick's view notifications (cleared at the start of step, not hashed)
    mealkit::sim::TickEvents<GameEvent, kMaxEventsPerTick> events;
};

// The character a player slot has (nullptr while waiting to respawn). Also used by the view side (read only)
const Pawn* playerPawn(const WorldState& state, u32 slot);

} // namespace sandbox
