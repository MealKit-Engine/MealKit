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
#include "cooldown/sim/Cooldown.h" // source module modules/cooldown (docs/MODULES.md)
#include "sim/Objects.h"
#include "sim/Tables.h"
#include "sim/Tuning.h"

#include <vector>

// Deterministic simulation of the sandbox game (physics: Jolt).
// sim/ folder rules (checked by SimCheck at build time): no CRT trig, real time, rand, renderer/platform headers, or mutable global state.
// Physics bodies are always created in the same order, and forces applied in the same order.
// State is declared with MEALKIT_FIELD: the hash (desync detection), snapshots (rewind/shadow simulation), and the editor [State] panel use the declarations as is.
// Undeclared members are excluded from the hash/snapshots, so values that persist across ticks must be MEALKIT_FIELDs.
// Objects created and destroyed mid-match use Pool<T>, "after N ticks" uses TickTimer, and things to tell the view use world.events (docs/CODING_MODEL.md).
namespace sandbox {

using mealkit::f32;
using mealkit::i8;
using mealkit::Quat;
using mealkit::u32;
using mealkit::u64;
using mealkit::u8;
using mealkit::Vec3;

inline constexpr u32 kPropColorCount = 6; // number of box colors (the colors themselves are in view tuning view/ViewTuning.h)

inline constexpr u8 kButtonJump = 1;
inline constexpr u8 kButtonPush = 2;

// Player input sent over the network (8 bytes).
// The client computes the move direction relative to the camera and quantizes it as a world direction → no trig needed in the simulation.
struct GameInput {
    i8 moveX = 0; // -127..127
    i8 moveZ = 0;
    u8 buttons = 0;
    u8 reserved[5] = {};
};
static_assert(sizeof(GameInput) == mealkit::session::kInputBytes);

// ---------------------------------------------------------------- Players and characters (docs/ROADMAP.md "Separating players and characters")
// PlayerSlot: a connected person. Persists through the match (score etc.).
// Pawn: a body in the world. Disappears when it falls, and a new one appears on respawn.
// Controller: connects a slot (person) or AI to a Pawn. Changed only via possess/unpossess (sim/Pawns.cpp).
using mealkit::sim::Id;
using mealkit::sim::TickTimer;
struct Controller;
inline constexpr u32 kAiPathPoints = 16; // waypoints kept per AI (a longer path is re-searched from its end)
struct Pawn;

enum class PawnKind : u8 { Runner }; // the movement mode comes from the body kind (tables/bodies.table movement)
enum class ControllerKind : u8 { None, Player, AI };

MEALKIT_STRUCT(Pawn) {
    MEALKIT_FIELD(PawnKind, kind, PawnKind::Runner, "종류");
    MEALKIT_FIELD(Id<Controller>, owner, {}, "조종하는 컨트롤러");
    MEALKIT_FIELD(Vec3, position, {}, "위치");
    MEALKIT_FIELD(Vec3, velocity, {}, "속도");
    MEALKIT_FIELD(cooldown::Cooldown, push, {}, "밀치기 쿨타임");
    MEALKIT_FIELD(mealkit::sim::Mover, mover, {}, "이동 (모드, 물리 바디, 땅에 닿음)");
};

MEALKIT_STRUCT(Controller) {
    MEALKIT_FIELD(ControllerKind, kind, ControllerKind::None, "종류");
    MEALKIT_FIELD(u32, slot, 0, "플레이어 슬롯"); // kind == Player
    MEALKIT_FIELD(u32, spawnIndex, 0, "스폰 번호");
    MEALKIT_FIELD(Id<Pawn>, pawn, {}, "빙의한 Pawn"); // if none, waiting to respawn (spectating)
    MEALKIT_FIELD(TickTimer, respawn, {}, "리스폰");
    MEALKIT_FIELD(Vec3, aiTarget, {}, "AI 목표 지점");
    MEALKIT_FIELD(TickTimer, aiThink, {}, "AI 다음 판단");
    MEALKIT_FIELD(bool, aiRepath, false, "AI 길찾기 대기"); // set when the target changes, searched within the per-tick limit
    MEALKIT_ARRAY(Vec3, kAiPathPoints, aiPath, "AI 경로 지점");
    MEALKIT_FIELD(u32, aiPathCount, 0, "AI 경로 지점 수");
    MEALKIT_FIELD(u32, aiPathNext, 0, "AI 다음 경로 지점");
};

MEALKIT_STRUCT(PlayerSlot) {
    MEALKIT_FIELD(bool, connected, false, "접속");
    MEALKIT_FIELD(u32, body, 0, "몸 종류 (bodies.table 행 번호, 매치 시작 데이터)");
    MEALKIT_FIELD(u32, team, 0, "팀");
    MEALKIT_FIELD(u32, falls, 0, "떨어진 횟수");
    MEALKIT_FIELD(u32, score, 0, "라운드 점수"); // +1 when another side's player falls
    MEALKIT_FIELD(u32, lives, 0, "남은 목숨 (tuning lives가 0이면 안 씀)");
    MEALKIT_FIELD(Id<Controller>, controller, {}, "컨트롤러");
};

// Scattered boxes
MEALKIT_STRUCT(Prop) {
    MEALKIT_FIELD(Vec3, position, {}, "위치");
    MEALKIT_FIELD(Quat, rotation, {}, "회전");
    MEALKIT_FIELD(Vec3, halfExtents, {}, "반 크기");
    MEALKIT_FIELD(u32, colorIndex, 0, "색 번호"); // 0..kPropColorCount-1
    MEALKIT_FIELD(mealkit::physics::BodyId, body, mealkit::physics::kInvalidBody, "물리 바디");
};

// The scene's jump pads (created from the scene at match start)
MEALKIT_STRUCT(Pad) {
    MEALKIT_FIELD(Vec3, center, {}, "중심");
    MEALKIT_FIELD(Vec3, halfExtents, {}, "반 크기");
    MEALKIT_FIELD(f32, power, 0.0f, "점프 힘");
    MEALKIT_FIELD(u32, cooldownTicks, 0, "재사용 대기");
    MEALKIT_FIELD(mealkit::sim::TickTimer, ready, {}, "다음 사용");
};

// Simulation → view notifications (sound/effects/shake). sim/ only writes them and never reads.
// It's a field declaration, so the editor [Feedback] panel shows kind names and fields, and the feedback table (assets/feedback.table) chooses feedback by kind name.
MEALKIT_ENUM(EventKind, u8, Push, Bounce, Spawn, Fall, Jump, Impact, Countdown, MatchStart, MatchEnd, RoundEnd);
inline constexpr u8 kNoPlayer = 0xFF;

MEALKIT_STRUCT(GameEvent) {
    MEALKIT_FIELD(EventKind, kind, EventKind::Push, "종류");
    MEALKIT_FIELD(u8, player, kNoPlayer, "플레이어 슬롯 (AI면 255)");
    MEALKIT_FIELD(Vec3, position, {}, "위치");
    MEALKIT_FIELD(f32, value, 0.0f, "밀치기: 범위 (m), 충돌: 다가오던 속도 (m/s), 카운트다운: 남은 초");
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

// Match phase (engine mealkit/sim/MatchFlow.h, driven by sim/Match.cpp): nobody moves outside Playing. The screen flow opens the results screen at Results
using MatchPhase = mealkit::sim::FlowPhase;

// The whole simulation state (the real state of physics bodies is inside PhysicsWorld; positions/rotations are copied here every tick).
// The view draws by interpolating the WorldState of the previous tick and this tick.
MEALKIT_STRUCT(WorldState) {
    MEALKIT_FIELD(u32, tick, 0, "틱");
    MEALKIT_FIELD(mealkit::sim::MatchFlow, flow, {}, "매치 흐름 (단계, 라운드, 승자)");
    MEALKIT_FIELD(u32, playerCount, 0, "플레이어 수");
    MEALKIT_ARRAY(PlayerSlot, mealkit::session::kMaxPlayers, slots, "플레이어 슬롯");
    MEALKIT_FIELD(mealkit::sim::Pool<Controller>, controllers, {}, "컨트롤러");
    MEALKIT_FIELD(mealkit::sim::Pool<Pawn>, pawns, {}, "Pawn");
    MEALKIT_FIELD(mealkit::Pcg32, aiRng, {}, "AI 난수");
    MEALKIT_FIELD(mealkit::sim::Pool<Prop>, props, {}, "상자");
    MEALKIT_FIELD(mealkit::sim::Pool<Pad>, pads, {}, "점프대");
};

// The whole simulation. sim/ system functions (Systems.h) take only this (World&).
struct World {
    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // physicsThreads = 0 matches the core count (results are independent of thread count).
    // Collision objects with models collide with the convex hull of the model's vertices.
    // Box count is the sum of the scene's box spawn areas (PropSpawnArea.count). If propOverride isn't 0, set to that count (--bodies, performance measurement).
    // Pool capacities are set at match start: boxes = box count, controllers = players + AI, Pawns = controllers × 2.
    // propKinds: box kind table (app/ reads it from tables/props.table and passes it)
    // playerBodies: bodies.table row index per player (loadout, 0 if none)
    void reset(u64 seed, u32 playerCount, u32 propOverride, const Tuning& tuning,
               const mealkit::sim::Table<PropKind>& propKinds, const mealkit::sim::Table<BodyKind>& bodyKinds,
               const std::vector<u32>& playerBodies, const mealkit::scene::Scene& scene, mealkit::asset::ModelCache& models,
               u32 tickRate, u32 physicsThreads = 0);
    // Changes tuning mid-match (editor solo play only: in multiplayer, differing per peer would desync)
    void setTuning(const Tuning& value);
    void step(const mealkit::session::TickInputs& inputs);
    u64 hash() const;
    u32 activeBodies() const { return physics.activeBodyCount(); }

    // Data decided at match start (from the project tick rate, scene, and tuning)
    u32 tickRate = 30;
    f32 tickDt = 1.0f / 30.0f; // const-ok: default (reset sets it from the project tick rate)
    struct PropSpawn {
        Vec3 min, max; // box spawn area
        u32 count = 0;
        u32 kind = 0; // row index in propKinds
    };
    std::vector<Vec3> spawns;
    std::vector<PropSpawn> propSpawns;
    Vec3 arenaMin, arenaMax; // match area (MatchArea): width/depth = AI/box range, bottom = death, top = dropping boxes again
    Tuning tuning;
    mealkit::sim::MatchRules rules; // from tuning (sim/Match.cpp matchRules)
    mealkit::sim::Table<PropKind> propKinds; // box kinds (a table that goes into the content hash)
    mealkit::sim::Table<BodyKind> bodyKinds; // character body kinds (a player slot's body is the row index)
    mealkit::sim::NavGrid nav;          // AI pathfinding grid baked from the scene's static bodies at reset (same on every peer, not state)
    mealkit::sim::NavSearch navSearch;  // A* scratch memory (not state)

    // State
    WorldState state;
    GameInput inputs[mealkit::session::kMaxPlayers] = {}; // this tick's inputs (filled at the start of step)
    mealkit::physics::PhysicsWorld physics;

    // This tick's view notifications (cleared at the start of step, not hashed)
    mealkit::sim::TickEvents<GameEvent, kMaxEventsPerTick> events;
};

// Lookups also used by the view side (read only)
// The Pawn a player slot currently controls (nullptr while waiting to respawn)
const Pawn* playerPawn(const WorldState& state, u32 slot);
// The player slot controlling a Pawn (kNoPlayer for AI or empty controllers)
u8 pawnPlayer(const WorldState& state, const Pawn& pawn);

} // namespace sandbox
