#pragma once

#include "mealkit/asset/ModelCache.h"
#include "mealkit/core/Math.h"
#include "mealkit/core/Random.h"
#include "mealkit/session/Session.h"
#include "mealkit/physics/PhysicsWorld.h"
#include "mealkit/scene/Scene.h"
#include "mealkit/sim/MatchFlow.h"
#include "mealkit/sim/Movement.h"
#include "mealkit/sim/NavGrid.h"
#include "mealkit/sim/Pool.h"
#include "mealkit/sim/TickEvents.h"
#include "mealkit/sim/TickTimer.h"
#include "sim/Objects.h"
#include "sim/Tables.h"
#include "sim/Tuning.h"

#include <vector>

// Deterministic simulation of the co-op game (physics: Jolt): 1-4 heroes cross a hill terrain to the goal while chasers hunt them.
// A caught hero is knocked down (active ragdoll) and stays down until a teammate stands next to them long enough.
// sim/ folder rules (checked by SimCheck at build time): no CRT trig, real time, rand, renderer/platform headers, or mutable global state.
// State is declared with MEALKIT_FIELD: the hash (desync detection), snapshots (rewind/shadow simulation), and the editor [State] panel use the declarations as is.
namespace coop {

using mealkit::f32;
using mealkit::i8;
using mealkit::Quat;
using mealkit::u32;
using mealkit::u64;
using mealkit::u8;
using mealkit::Vec3;

inline constexpr u8 kButtonJump = 1;
inline constexpr u8 kButtonPush = 2; // shove: stuns chasers nearby

// Player input sent over the network (8 bytes). The move direction is already a world direction (camera relative on the view side).
struct GameInput {
    i8 moveX = 0; // -127..127
    i8 moveZ = 0;
    u8 buttons = 0;
    u8 reserved[5] = {};
};
static_assert(sizeof(GameInput) == mealkit::session::kInputBytes);

// ---------------------------------------------------------------- Players, controllers, Pawns (same split as samples/sandbox)
using mealkit::sim::Id;
using mealkit::sim::TickTimer;
struct Controller;
struct Pawn;
inline constexpr u32 kAiPathPoints = 16; // waypoints kept per chaser (a longer path is searched again from its end)

MEALKIT_ENUM(PawnKind, u8, Hero, Chaser); // enums with names: the editor [State] panel and scenario tests read them by name
MEALKIT_ENUM(ControllerKind, u8, None, Player, Chaser);

MEALKIT_STRUCT(Pawn) {
    MEALKIT_FIELD(PawnKind, kind, PawnKind::Hero, "종류");
    MEALKIT_FIELD(Id<Controller>, owner, {}, "조종하는 컨트롤러");
    MEALKIT_FIELD(Vec3, position, {}, "위치");
    MEALKIT_FIELD(Vec3, velocity, {}, "속도");
    MEALKIT_FIELD(mealkit::sim::Mover, mover, {}, "이동 (모드, 물리 바디, 땅에 닿음)");
    MEALKIT_FIELD(TickTimer, shove, {}, "밀치기 재사용");      // heroes
    MEALKIT_FIELD(bool, downed, false, "쓰러짐");               // heroes: caught, waiting for a teammate
    MEALKIT_FIELD(u32, revive, 0, "부활 진행 (틱)");            // heroes: ticks a teammate has stood next to them
    MEALKIT_FIELD(TickTimer, stunned, {}, "기절");              // chasers: shoved or just caught someone
};

MEALKIT_STRUCT(Controller) {
    MEALKIT_FIELD(ControllerKind, kind, ControllerKind::None, "종류");
    MEALKIT_FIELD(u32, slot, 0, "플레이어 슬롯");   // kind == Player
    MEALKIT_FIELD(u32, spawnIndex, 0, "스폰 번호"); // heroes: scene spawn point, chasers: chaser spawn index
    MEALKIT_FIELD(Id<Pawn>, pawn, {}, "빙의한 Pawn");
    MEALKIT_FIELD(TickTimer, respawn, {}, "리스폰");
    MEALKIT_FIELD(Id<Pawn>, prey, {}, "추격 대상");  // chasers
    MEALKIT_FIELD(TickTimer, aiThink, {}, "다음 판단");
    MEALKIT_FIELD(bool, aiRepath, false, "길찾기 대기");
    MEALKIT_ARRAY(Vec3, kAiPathPoints, aiPath, "경로 지점");
    MEALKIT_FIELD(u32, aiPathCount, 0, "경로 지점 수");
    MEALKIT_FIELD(u32, aiPathNext, 0, "다음 경로 지점");
};

MEALKIT_STRUCT(PlayerSlot) {
    MEALKIT_FIELD(bool, connected, false, "접속");
    MEALKIT_FIELD(u32, body, 0, "몸 종류 (bodies.table 행 번호)");
    MEALKIT_FIELD(u32, downs, 0, "쓰러진 횟수");
    MEALKIT_FIELD(u32, revives, 0, "동료를 일으킨 횟수");
    MEALKIT_FIELD(Id<Controller>, controller, {}, "컨트롤러");
};

// Simulation → view notifications. sim/ only writes them. Kind names are rows of assets/feedback.table.
MEALKIT_ENUM(EventKind, u8, Push, Spawn, Fall, Jump, Impact, Caught, Revive, Stunned, Countdown, MatchStart, MatchEnd,
             RoundEnd);
inline constexpr u8 kNoPlayer = 0xFF;

MEALKIT_STRUCT(GameEvent) {
    MEALKIT_FIELD(EventKind, kind, EventKind::Push, "종류");
    MEALKIT_FIELD(u8, player, kNoPlayer, "플레이어 슬롯 (추격자면 255)");
    MEALKIT_FIELD(Vec3, position, {}, "위치");
    MEALKIT_FIELD(f32, value, 0.0f, "밀치기: 범위 (m), 충돌: 속도 (m/s), 카운트다운: 남은 초, 라운드 끝: 1 성공 0 실패");
};

inline GameEvent makeEvent(EventKind kind, u8 player, Vec3 position, f32 value) {
    GameEvent e;
    e.kind = kind;
    e.player = player;
    e.position = position;
    e.value = value;
    return e;
}
inline constexpr u32 kMaxEventsPerTick = 64;

using MatchPhase = mealkit::sim::FlowPhase;

MEALKIT_STRUCT(WorldState) {
    MEALKIT_FIELD(u32, tick, 0, "틱");
    MEALKIT_FIELD(mealkit::sim::MatchFlow, flow, {}, "매치 흐름 (단계, 라운드, 승패)");
    MEALKIT_FIELD(u32, playerCount, 0, "플레이어 수");
    MEALKIT_ARRAY(PlayerSlot, mealkit::session::kMaxPlayers, slots, "플레이어 슬롯");
    MEALKIT_FIELD(mealkit::sim::Pool<Controller>, controllers, {}, "컨트롤러");
    MEALKIT_FIELD(mealkit::sim::Pool<Pawn>, pawns, {}, "Pawn");
    MEALKIT_FIELD(mealkit::Pcg32, aiRng, {}, "AI 난수");
    MEALKIT_FIELD(u32, goalTicks, 0, "모두 목표 안에 있었던 틱");
};

struct World {
    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // playerBodies: bodies.table row index per player (loadout, 0 if none). physicsThreads = 0 matches the core count.
    void reset(u64 seed, u32 playerCount, const Tuning& tuning, const mealkit::sim::Table<BodyKind>& bodyKinds,
               const std::vector<u32>& playerBodies, const mealkit::scene::Scene& scene, mealkit::asset::ModelCache& models,
               u32 tickRate, u32 physicsThreads = 0);
    void setTuning(const Tuning& value); // editor solo play only
    void step(const mealkit::session::TickInputs& inputs);
    u64 hash() const;
    u32 activeBodies() const { return physics.activeBodyCount(); }

    // Data decided at match start (scene, tuning)
    u32 tickRate = 30;
    f32 tickDt = 1.0f / 30.0f; // const-ok: default (reset sets it from the project tick rate)
    struct ChaserSpawn {
        Vec3 position;
        u32 count = 0;
    };
    std::vector<Vec3> spawns;             // hero spawn points (scene spawn objects, also where fallen heroes come back)
    std::vector<ChaserSpawn> chaserSpawns; // ChaserSpawn objects in scene order
    Vec3 arenaMin, arenaMax;               // MatchArea: falling below the bottom = back to a spawn point
    Vec3 goalMin, goalMax;                 // GoalArea: everyone up inside it wins the round
    bool hasGoal = false;
    Tuning tuning;
    mealkit::sim::MatchRules rules;
    mealkit::sim::Table<BodyKind> bodyKinds;
    mealkit::sim::NavGrid nav;         // baked from the static scene (terrain, rocks) at reset, same on every peer
    mealkit::sim::NavSearch navSearch; // A* scratch memory (not state)

    WorldState state;
    GameInput inputs[mealkit::session::kMaxPlayers] = {};
    mealkit::physics::PhysicsWorld physics;
    mealkit::sim::TickEvents<GameEvent, kMaxEventsPerTick> events; // cleared at the start of step, not hashed
};

// Lookups also used by the view side (read only)
const Pawn* playerPawn(const WorldState& state, u32 slot); // nullptr while waiting to respawn
u8 pawnPlayer(const WorldState& state, const Pawn& pawn);  // kNoPlayer for chasers
const BodyKind* bodyOf(const World& world, u8 player);     // a hero's body kind (nullptr for chasers or an empty table)
// Revive progress of a downed hero (0..1)
f32 reviveProgress(const World& world, const Pawn& pawn);

} // namespace coop
