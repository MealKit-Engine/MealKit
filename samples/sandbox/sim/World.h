#pragma once

#include "mealkit/asset/ModelCache.h"
#include "mealkit/core/Math.h"
#include "mealkit/core/Random.h"
#include "mealkit/net/Lockstep.h"
#include "mealkit/physics/PhysicsWorld.h"
#include "mealkit/scene/Scene.h"
#include "mealkit/sim/Pool.h"
#include "mealkit/sim/TickEvents.h"
#include "mealkit/sim/TickTimer.h"
#include "sim/Objects.h"
#include "sim/Tables.h"
#include "sim/Tuning.h"

#include <vector>

// 샌드박스 게임의 결정론적 시뮬레이션 (물리: Jolt).
// sim/ 폴더 규칙 (빌드 때 SimCheck가 검사한다): CRT 삼각함수·실제 시간·rand·렌더러/플랫폼 헤더·전역 가변 상태 금지.
// 물리 바디는 항상 같은 순서로 만들고, 힘도 같은 순서로 준다.
// 상태는 MEALKIT_FIELD로 선언한다: 해시(디싱크 감지), 스냅샷(되감기·그림자 시뮬레이션), 에디터 [상태] 패널이 선언을 그대로 쓴다.
// 선언하지 않은 멤버는 해시·스냅샷에서 빠지므로, 틱을 넘어 남는 값은 반드시 MEALKIT_FIELD로 둔다.
// 매치 중에 생기고 사라지는 객체는 Pool<T>, "N틱 뒤"는 TickTimer, 화면에 알릴 일은 world.events (docs/CODING_MODEL.md).
namespace sandbox {

using mealkit::f32;
using mealkit::i8;
using mealkit::Quat;
using mealkit::u32;
using mealkit::u64;
using mealkit::u8;
using mealkit::Vec3;

inline constexpr u32 kPropColorCount = 6; // 상자 색 종류 수 (색 자체는 화면용 튜닝 view/ViewTuning.h)

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
static_assert(sizeof(GameInput) == mealkit::net::kInputBytes);

// ---------------------------------------------------------------- 플레이어와 캐릭터 (docs/ROADMAP.md "플레이어와 캐릭터 분리")
// PlayerSlot: 접속한 사람. 매치 동안 남는다 (점수 등).
// Pawn: 월드 안의 몸. 떨어지면 사라지고, 리스폰하면 새로 생긴다.
// Controller: 슬롯(사람) 또는 AI를 Pawn에 잇는다. possess/unpossess로만 바꾼다 (sim/Pawns.cpp).
using mealkit::sim::Id;
using mealkit::sim::TickTimer;
struct Controller;
struct Pawn;

enum class PawnKind : u8 { Runner }; // 지금은 회전이 잠긴 물리 캡슐 하나
enum class ControllerKind : u8 { None, Player, AI };

MEALKIT_STRUCT(Pawn) {
    MEALKIT_FIELD(PawnKind, kind, PawnKind::Runner, "종류");
    MEALKIT_FIELD(Id<Controller>, owner, {}, "조종하는 컨트롤러");
    MEALKIT_FIELD(Vec3, position, {}, "위치");
    MEALKIT_FIELD(Vec3, velocity, {}, "속도");
    MEALKIT_FIELD(u32, grounded, 0, "땅에 닿음"); // 발밑에 무언가 있는가
    MEALKIT_FIELD(TickTimer, pushReady, {}, "다음 밀치기");
    MEALKIT_FIELD(mealkit::physics::BodyId, body, mealkit::physics::kInvalidBody, "물리 바디");
};

MEALKIT_STRUCT(Controller) {
    MEALKIT_FIELD(ControllerKind, kind, ControllerKind::None, "종류");
    MEALKIT_FIELD(u32, slot, 0, "플레이어 슬롯"); // kind == Player
    MEALKIT_FIELD(u32, spawnIndex, 0, "스폰 번호");
    MEALKIT_FIELD(Id<Pawn>, pawn, {}, "빙의한 Pawn"); // 없으면 리스폰 대기(관전)
    MEALKIT_FIELD(TickTimer, respawn, {}, "리스폰");
    MEALKIT_FIELD(Vec3, aiTarget, {}, "AI 목표 지점");
    MEALKIT_FIELD(TickTimer, aiThink, {}, "AI 다음 판단");
};

MEALKIT_STRUCT(PlayerSlot) {
    MEALKIT_FIELD(bool, connected, false, "접속");
    MEALKIT_FIELD(u32, body, 0, "몸 종류 (bodies.table 행 번호, 매치 시작 데이터)");
    MEALKIT_FIELD(u32, team, 0, "팀");
    MEALKIT_FIELD(u32, falls, 0, "떨어진 횟수");
    MEALKIT_FIELD(Id<Controller>, controller, {}, "컨트롤러");
};

// 흩어진 상자
MEALKIT_STRUCT(Prop) {
    MEALKIT_FIELD(Vec3, position, {}, "위치");
    MEALKIT_FIELD(Quat, rotation, {}, "회전");
    MEALKIT_FIELD(Vec3, halfExtents, {}, "반 크기");
    MEALKIT_FIELD(u32, colorIndex, 0, "색 번호"); // 0..kPropColorCount-1
    MEALKIT_FIELD(mealkit::physics::BodyId, body, mealkit::physics::kInvalidBody, "물리 바디");
};

// 씬의 점프대 (매치 시작 때 씬에서 만든다)
MEALKIT_STRUCT(Pad) {
    MEALKIT_FIELD(Vec3, center, {}, "중심");
    MEALKIT_FIELD(Vec3, halfExtents, {}, "반 크기");
    MEALKIT_FIELD(f32, power, 0.0f, "점프 힘");
    MEALKIT_FIELD(u32, cooldownTicks, 0, "재사용 대기");
    MEALKIT_FIELD(mealkit::sim::TickTimer, ready, {}, "다음 사용");
};

// 시뮬레이션 → 화면 알림 (소리·효과·흔들림). sim/은 쓰기만 하고 읽지 않는다.
// 필드 선언이라 에디터 [연출] 패널이 종류 이름과 필드를 보여 주고, 연출 표(assets/feedback.table)가 종류 이름으로 연출을 고른다.
MEALKIT_ENUM(EventKind, u8, Push, Bounce, Spawn, Fall, Jump, Impact, Countdown, MatchStart, MatchEnd);
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

// 매치 단계 (sim/Match.cpp): 카운트다운 동안과 끝난 뒤에는 아무도 움직이지 않는다. 화면 흐름은 Finished면 결과 화면을 연다
MEALKIT_ENUM(MatchPhase, u8, Countdown, Playing, Finished);

// 시뮬레이션 상태 전체 (물리 바디의 실제 상태는 PhysicsWorld 안에 있고, 위치·회전은 매 틱 여기로 옮겨 담는다).
// 화면은 직전 틱과 이번 틱의 WorldState를 보간해 그린다.
MEALKIT_STRUCT(WorldState) {
    MEALKIT_FIELD(u32, tick, 0, "틱");
    MEALKIT_FIELD(MatchPhase, phase, MatchPhase::Countdown, "매치 단계");
    MEALKIT_FIELD(TickTimer, phaseEnd, {}, "단계가 끝나는 틱 (카운트다운, 경기 시간)");
    MEALKIT_FIELD(u32, playerCount, 0, "플레이어 수");
    MEALKIT_ARRAY(PlayerSlot, mealkit::net::kMaxPlayers, slots, "플레이어 슬롯");
    MEALKIT_FIELD(mealkit::sim::Pool<Controller>, controllers, {}, "컨트롤러");
    MEALKIT_FIELD(mealkit::sim::Pool<Pawn>, pawns, {}, "Pawn");
    MEALKIT_FIELD(mealkit::Pcg32, aiRng, {}, "AI 난수");
    MEALKIT_FIELD(mealkit::sim::Pool<Prop>, props, {}, "상자");
    MEALKIT_FIELD(mealkit::sim::Pool<Pad>, pads, {}, "점프대");
};

// 시뮬레이션 전체. sim/ 시스템 함수(Systems.h)는 이것 하나(World&)만 받는다.
struct World {
    World() = default;
    World(const World&) = delete;
    World& operator=(const World&) = delete;

    // physicsThreads = 0 이면 코어 수에 맞춘다 (결과는 스레드 수와 무관하다).
    // 모델이 있는 충돌 물체는 모델 정점의 볼록 껍질로 충돌한다.
    // 상자 수는 씬의 상자 생성 영역(PropSpawnArea.count)의 합. propOverride가 0이 아니면 그 수로 맞춘다(--bodies, 성능 측정).
    // Pool 용량은 매치 시작 때 정한다: 상자 = 상자 수, 컨트롤러 = 플레이어 + AI, Pawn = 컨트롤러 × 2.
    // propKinds: 상자 종류 표 (app/이 tables/props.table에서 읽어 넘긴다)
    // playerBodies: 플레이어마다 bodies.table 행 번호 (로드아웃, 없으면 0)
    void reset(u64 seed, u32 playerCount, u32 propOverride, const Tuning& tuning,
               const mealkit::sim::Table<PropKind>& propKinds, const mealkit::sim::Table<BodyKind>& bodyKinds,
               const std::vector<u32>& playerBodies, const mealkit::scene::Scene& scene, mealkit::asset::ModelCache& models,
               u32 tickRate, u32 physicsThreads = 0);
    // 매치 도중 튜닝을 바꾼다 (에디터 혼자 플레이 전용: 멀티플레이에서는 피어마다 달라지면 디싱크)
    void setTuning(const Tuning& value);
    void step(const mealkit::net::TickInputs& inputs);
    u64 hash() const;
    u32 activeBodies() const { return physics.activeBodyCount(); }

    // 매치 시작 때 정해지는 데이터 (프로젝트 틱레이트, 씬과 튜닝에서)
    u32 tickRate = 30;
    f32 tickDt = 1.0f / 30.0f; // const-ok: 기본값 (reset이 프로젝트 틱레이트로 정한다)
    struct PropSpawn {
        Vec3 min, max; // 상자 생성 영역
        u32 count = 0;
        u32 kind = 0; // propKinds의 행 번호
    };
    std::vector<Vec3> spawns;
    std::vector<PropSpawn> propSpawns;
    Vec3 arenaMin, arenaMax; // 경기 영역 (MatchArea): 가로·세로 = AI·상자 범위, 바닥 = 낙사, 위 = 상자 다시 떨어뜨리기
    Tuning tuning;
    mealkit::sim::Table<PropKind> propKinds; // 상자 종류 (콘텐츠 해시에 들어가는 표)
    mealkit::sim::Table<BodyKind> bodyKinds; // 캐릭터 몸 종류 (플레이어 슬롯의 body가 행 번호)

    // 상태
    WorldState state;
    GameInput inputs[mealkit::net::kMaxPlayers] = {}; // 이번 틱 입력 (step 시작 때 채운다)
    mealkit::physics::PhysicsWorld physics;

    // 이번 틱의 화면 알림 (step 시작 때 비운다, 해시에 넣지 않는다)
    mealkit::sim::TickEvents<GameEvent, kMaxEventsPerTick> events;
};

// 화면 쪽에서도 쓰는 조회 (읽기만 한다)
// 플레이어 슬롯이 지금 조종하는 Pawn (리스폰 대기 중이면 nullptr)
const Pawn* playerPawn(const WorldState& state, u32 slot);
// Pawn을 조종하는 플레이어 슬롯 (AI나 빈 컨트롤러면 kNoPlayer)
u8 pawnPlayer(const WorldState& state, const Pawn& pawn);

} // namespace sandbox
