// 엔진 단위 테스트 (mealkit_tests.exe). 엔진 저장소 빌드에만 있다. tools\check.ps1이 Release와 Editor 빌드에서 돌린다.
//   mealkit_tests.exe          모든 테스트, 끝에 PASS 또는 FAILED
//   출력의 "detmath-bits <hash>" 줄은 결정론 수학 결과 비트의 해시다. 빌드 구성이 달라도 같아야 한다.
#include "mealkit/core/DetMath.h"
#include "mealkit/core/Hash.h"
#include "mealkit/core/Log.h"
#include "mealkit/core/Random.h"
#include "mealkit/game/InputRecord.h"
#include "mealkit/game/ProjectSettings.h"
#include "mealkit/game/Simulation.h"
#include "mealkit/net/Lockstep.h"
#include "mealkit/core/Reflect.h"
#include "mealkit/physics/PhysicsWorld.h"
#include "mealkit/platform/Paths.h"
#include "mealkit/scene/Scene.h"
#include "mealkit/sim/Pool.h"
#include "mealkit/sim/TickEvents.h"
#include "mealkit/sim/TickTimer.h"

#include "TestRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>

using namespace mealkit;

namespace {

struct Item {
    u32 value = 0;
};

u32 countIter(const sim::Pool<Item>& p) {
    u32 n = 0;
    for (auto [id, it] : p) {
        (void)it;
        TEST_CHECK(p.get(id) != nullptr);
        ++n;
    }
    return n;
}

TEST_CASE(pool) {
    using sim::Id;
    using sim::Pool;
    Pool<Item> p;
    p.init(3);
    const Id<Item> a = p.create({1});
    const Id<Item> b = p.create({2});
    TEST_CHECK(a.slot == 0 && b.slot == 1);
    TEST_CHECK(p.get(a) && p.get(a)->value == 1); // 만든 틱에도 get은 된다
    TEST_CHECK(countIter(p) == 0);                // 만든 틱에는 순회에 안 나온다
    p.endTick();
    TEST_CHECK(countIter(p) == 2);

    u32 seen = 0;
    for (auto [id, it] : p) { // 순회 중 생성: 이번 틱 순회에 안 나온다
        ++seen;
        if (it.value == 1) p.create({3});
    }
    TEST_CHECK(seen == 2);
    TEST_CHECK(p.count() == 3);

    p.destroy(a);
    p.destroy(a); // 두 번 지워도 무시
    TEST_CHECK(p.get(a) == nullptr);
    TEST_CHECK(countIter(p) == 1);
    u32 destroyed = 0;
    p.endTick([&](Item& it) {
        TEST_CHECK(it.value == 1);
        ++destroyed;
    });
    TEST_CHECK(destroyed == 1 && p.count() == 2 && countIter(p) == 2);

    const Id<Item> c = p.create({5}); // 슬롯 재사용: 옛 id는 세대가 달라 무효
    TEST_CHECK(c.slot == 0 && c.generation == 2);
    TEST_CHECK(p.get(a) == nullptr && p.get(c)->value == 5);
    p.destroy(c); // 만든 틱에 지우기
    p.endTick();
    TEST_CHECK(p.count() == 2 && p.get(c) == nullptr);

    Pool<Item> x, y; // 지운 순서와 관계없이 같은 결과
    x.init(4);
    y.init(4);
    Id<Item> xi[4], yi[4];
    for (u32 i = 0; i < 4; ++i) {
        xi[i] = x.create({i});
        yi[i] = y.create({i});
    }
    x.endTick();
    y.endTick();
    x.destroy(xi[3]);
    x.destroy(xi[1]);
    y.destroy(yi[1]);
    y.destroy(yi[3]);
    x.endTick();
    y.endTick();
    Hasher hx, hy;
    x.hashState(hx);
    y.hashState(hy);
    TEST_CHECK(hx.value == hy.value);
    TEST_CHECK(x.create().slot == y.create().slot);
}

TEST_CASE(tick_timer_and_events) {
    sim::TickTimer t;
    TEST_CHECK(t.ready(0));
    t.start(10, 15);
    TEST_CHECK(!t.ready(24) && t.ready(25) && t.remaining(20) == 5 && t.remaining(30) == 0);

    sim::TickEvents<Item, 2> ev;
    ev.push({1});
    ev.push({2});
    TEST_CHECK(ev.size() == 2);
    ev.clear();
    TEST_CHECK(ev.size() == 0 && ev.begin() == ev.end());
}

TEST_CASE(det_math) {
    f32 worstSin = 0.0f, worstCos = 0.0f, worstAtan = 0.0f;
    Hasher bits;
    for (i32 i = -200000; i <= 200000; ++i) {
        const f32 x = static_cast<f32>(i) * 0.0005f; // -100..100 라디안
        const f32 s = detSin(x), c = detCos(x);
        worstSin = std::max(worstSin, std::fabs(s - std::sin(x)));
        worstCos = std::max(worstCos, std::fabs(c - std::cos(x)));
        bits.add(s);
        bits.add(c);
    }
    for (i32 iy = -60; iy <= 60; ++iy) {
        for (i32 ix = -60; ix <= 60; ++ix) {
            const f32 y = static_cast<f32>(iy) * 0.37f, x = static_cast<f32>(ix) * 0.41f;
            const f32 a = detAtan2(y, x);
            worstAtan = std::max(worstAtan, std::fabs(a - std::atan2(y, x)));
            bits.add(a);
        }
    }
    const Quat q = detQuatFromAxisAngle({0, 1, 0}, 1.0f);
    bits.add(q);
    std::printf("  det_math worst error: sin %.2e cos %.2e atan2 %.2e\n", worstSin, worstCos, worstAtan);
    std::printf("detmath-bits %016llx\n", static_cast<unsigned long long>(bits.value));
    TEST_CHECK(worstSin < 2e-7f && worstCos < 2e-7f);
    TEST_CHECK(worstAtan < 5e-7f);
    TEST_CHECK(detSin(0.0f) == 0.0f && detCos(0.0f) == 1.0f);
    TEST_CHECK(detAtan2(0.0f, 0.0f) == 0.0f && detAtan2(1.0f, 0.0f) == kDetHalfPi);
    TEST_CHECK(std::fabs(detAtan2(0.0f, -1.0f) - kDetPi) < 1e-6f);
}

enum class Color8 : u8 { Red, Green };

MEALKIT_STRUCT(Inner) {
    MEALKIT_FIELD(i32, a, -3, "에이");
    MEALKIT_FIELD(Vec3, v, (Vec3{1.0f, 2.0f, 3.0f}), "");
};

MEALKIT_STRUCT(Outer) {
    MEALKIT_FIELD(u8, small, 7, "작은 값");
    MEALKIT_FIELD(bool, flag, true, "깃발");
    MEALKIT_VIEW_FIELD(f32, look, 0.25f, "보기만");
    MEALKIT_FIELD(Inner, inner, {}, "안쪽");
    MEALKIT_ARRAY(Inner, 2, list, "목록");
    MEALKIT_FIELD(sim::Pool<Inner>, pool, {}, "풀");
    MEALKIT_FIELD(sim::Id<Inner>, ref, {}, "참조");
    MEALKIT_FIELD(Color8, color, Color8::Green, "색");
    MEALKIT_FIELD(sim::TickTimer, timer, {}, "타이머");
    u32 notReflected = 99; // 선언하지 않은 필드는 해시·저장에서 빠진다
};

MEALKIT_OBJECT(TestPad) {
    static constexpr const char* kObjectLabel = "시험 발판";
    static constexpr u32 kObjectFlags = reflect::kObjectMarker;
    MEALKIT_FIELD(f32, power, 12.0f, "힘");
};

TEST_CASE(reflect) {
    using reflect::Kind;
    const reflect::TypeInfo& t = reflect::typeOf<Outer>();
    TEST_CHECK(std::string(t.name) == "Outer");
    TEST_CHECK(t.fields.size() == 9);
    TEST_CHECK(std::string(t.fields[0].name) == "small" && t.fields[0].value->kind == Kind::UInt &&
               t.fields[0].value->size == 1);
    TEST_CHECK(t.fields[2].flags == reflect::kFieldView && t.fields[2].value->kind == Kind::Float);
    TEST_CHECK(t.fields[3].value->kind == Kind::Struct && t.fields[3].value->type == &reflect::typeOf<Inner>());
    TEST_CHECK(t.fields[4].value->kind == Kind::Array && t.fields[4].value->count == 2);
    TEST_CHECK(t.fields[5].value->kind == Kind::Container && t.fields[6].value->kind == Kind::Id);
    TEST_CHECK(t.fields[7].value->kind == Kind::UInt && t.fields[8].value->kind == Kind::Struct);
    TEST_CHECK(t.fields[1].offset == offsetof(Outer, flag) && t.fields[4].offset == offsetof(Outer, list));
    TEST_CHECK(std::string(reflect::typeOf<Inner>().fields[1].label) == "v"); // 빈 라벨은 이름

    Outer a;
    a.pool.init(4);
    a.pool.create({});
    a.pool.endTick();
    a.list[1].a = 42;
    reflect::TextFields text;
    reflect::toText(t, &a, text);
    TEST_CHECK(!text.empty() && text[0].first == "small" && text[0].second == "7");
    bool sawNested = false;
    for (const auto& [k, v] : text)
        if (k == "list[1].a") sawNested = v == "42";
    TEST_CHECK(sawNested);

    Outer b;
    b.pool.init(4);
    b.pool.create({});
    b.pool.endTick();
    std::string warnings;
    reflect::fromText(t, &b, text, &warnings);
    TEST_CHECK(warnings.empty());
    TEST_CHECK(b.list[1].a == 42);
    TEST_CHECK(reflect::hashOf(a) == reflect::hashOf(b));
    b.look = 9.0f;
    TEST_CHECK(reflect::hashOf(a, reflect::kFieldView) == reflect::hashOf(b, reflect::kFieldView));
    TEST_CHECK(reflect::hashOf(a) != reflect::hashOf(b));
    TEST_CHECK(!reflect::setFromText(t, &b, "list[5].a", "1") && !reflect::setFromText(t, &b, "nope", "1"));
    TEST_CHECK(reflect::setFromText(t, &b, "inner.v", "4 5 6") && b.inner.v.y == 5.0f);

    std::vector<std::string> diff;
    reflect::diffStruct(t, &a, &b, diff);
    TEST_CHECK(diff.size() == 2); // look, inner.v

    // 스냅샷: 저장 → 바꾸기 → 되돌리기
    const u64 saved = reflect::hashOf(a);
    reflect::ByteWriter w;
    reflect::saveStruct(t, &a, w);
    const sim::Id<Inner> extra = a.pool.create({});
    a.pool.endTick();
    a.timer.start(5, 10);
    a.notReflected = 1;
    const u64 changed = reflect::hashOf(a);
    reflect::ByteReader r(w.data().data(), w.data().size());
    TEST_CHECK(reflect::loadStruct(t, &a, r) && r.atEnd());
    TEST_CHECK(!a.pool.alive(extra) && a.pool.count() == 1 && a.timer.readyTick == 0);
    TEST_CHECK(changed != saved && reflect::hashOf(a) == saved);

    bool registered = false;
    for (const reflect::TypeInfo* type : reflect::moduleObjectTypes())
        if (std::string(type->name) == "TestPad")
            registered = std::string(type->label) == "시험 발판" && type->flags == reflect::kObjectMarker;
    TEST_CHECK(registered);
    reflect::Instance inst(reflect::typeOf<TestPad>());
    TEST_CHECK(static_cast<const TestPad*>(inst.data())->power == 12.0f);
}

MEALKIT_STRUCT(Named) {
    MEALKIT_FIELD(std::string, title, "기본 이름", "이름");
    MEALKIT_FIELD(u32, count, 3, "개수");
};

MEALKIT_STRUCT(HoldsNamed) {
    MEALKIT_ARRAY(Named, 2, items, "목록");
};

static_assert(reflect::hasStringField<Named>() && reflect::hasStringField<HoldsNamed>());
static_assert(!reflect::hasStringField<Inner>() && !reflect::hasStringField<Outer>());

TEST_CASE(reflect_string) {
    const reflect::TypeInfo& t = reflect::typeOf<Named>();
    TEST_CHECK(t.fields[0].value->kind == reflect::Kind::String);
    Named a;
    TEST_CHECK(reflect::setFromText(t, &a, "title", "D:\\My Games\\x y") && a.title == "D:\\My Games\\x y");
    reflect::TextFields text;
    reflect::toText(t, &a, text);
    TEST_CHECK(text[0].first == "title" && text[0].second == a.title);
    Named b;
    TEST_CHECK(reflect::hashOf(a) != reflect::hashOf(b));
    reflect::fromText(t, &b, text);
    TEST_CHECK(b.title == a.title && reflect::hashOf(a) == reflect::hashOf(b));
    // 길이도 해시에 들어간다 ("ab"+"c" 와 "a"+"bc"가 같지 않게)
    HoldsNamed x, y;
    x.items[0].title = "ab";
    x.items[1].title = "c";
    y.items[0].title = "a";
    y.items[1].title = "bc";
    TEST_CHECK(reflect::hashOf(x) != reflect::hashOf(y));
    std::vector<std::string> diff;
    reflect::diffStruct(reflect::typeOf<HoldsNamed>(), &x, &y, diff);
    TEST_CHECK(diff.size() == 2);

    reflect::ByteWriter w;
    reflect::saveStruct(t, &a, w);
    Named c;
    reflect::ByteReader r(w.data().data(), w.data().size());
    TEST_CHECK(reflect::loadStruct(t, &c, r) && r.atEnd() && c.title == a.title);
}

TEST_CASE(input_record) {
    const std::string path = executableDirectory() + "/test_inputs.rec";
    const game::MatchInfo match{0x0123456789ABCDEFull, 3, 2000};
    std::vector<net::TickInputs> inputs(5);
    for (u32 t = 0; t < inputs.size(); ++t)
        for (u32 p = 0; p < net::kMaxPlayers; ++p)
            for (u32 b = 0; b < net::kInputBytes; ++b) inputs[t].players[p].bytes[b] = static_cast<u8>(t * 31 + p * 7 + b);
    {
        game::InputRecordWriter w;
        TEST_CHECK(w.open(path, match, 0xFEEDull));
        for (u32 t = 0; t < inputs.size(); ++t) w.write(inputs[t], 1000ull + t);
    }
    game::InputRecording rec;
    std::string error;
    TEST_CHECK(game::loadInputRecording(path, rec, error));
    TEST_CHECK(rec.match.seed == match.seed && rec.match.playerCount == 3 && rec.match.matchData == 2000);
    TEST_CHECK(rec.contentHash == 0xFEEDull && rec.inputs.size() == 5 && rec.hashes.size() == 5);
    bool same = true;
    for (u32 t = 0; t < inputs.size(); ++t) {
        same = same && rec.hashes[t] == 1000ull + t;
        for (u32 p = 0; p < net::kMaxPlayers; ++p) // 인원 밖의 플레이어는 기록하지 않는다 (0으로 읽힌다)
            for (u32 b = 0; b < net::kInputBytes; ++b)
                same = same && rec.inputs[t].players[p].bytes[b] == (p < 3 ? inputs[t].players[p].bytes[b] : 0);
    }
    TEST_CHECK(same);
    removeFile(path);
}

TEST_CASE(project_file) {
    const std::string path = executableDirectory() + "\\test.project";
    // 예전 형식(키=값)과 새 형식(경로 값)이 섞여 있어도 읽는다
    TEST_CHECK(writeTextFile(path, "# header line\r\nname=Old\r\nengine = D:\\My Engine\r\n"
                                   "scene scenes/b.scene\r\ngame.port 99\r\nfutureKey 1 2\r\n"));
    game::ProjectFile p;
    TEST_CHECK(game::loadProjectFile(path, p));
    TEST_CHECK(p.settings.name == "Old" && p.settings.engine == "D:\\My Engine" && p.settings.scene == "scenes/b.scene");
    TEST_CHECK(p.gameFields.size() == 1 && p.gameFields[0].first == "port" && p.gameFields[0].second == "99");
    TEST_CHECK(p.unknownFields.size() == 1 && p.unknownFields[0].second == "1 2");
    TEST_CHECK(p.header == "# header line\n");
    TEST_CHECK(game::startSceneAsset(p.settings) == "assets/scenes/b.scene");

    // 저장하면 새 형식, 다시 읽으면 같은 값
    TEST_CHECK(game::saveProjectFile(path, p));
    std::string saved;
    TEST_CHECK(readTextFile(path, saved) && saved.find("engine D:\\My Engine\n") != std::string::npos &&
               saved.find("game.port 99\n") != std::string::npos);
    game::ProjectFile q;
    TEST_CHECK(game::loadProjectFile(path, q));
    TEST_CHECK(q.settings.name == "Old" && q.settings.engine == p.settings.engine && q.gameFields == p.gameFields &&
               q.unknownFields == p.unknownFields && q.header == p.header);
    removeFile(path);

    // 엔진 항목은 모두 해시 제외: 해시 대상만 콘텐츠 해시에 들어간다
    game::ProjectSettings other = q.settings;
    other.engine = "E:\\elsewhere";
    TEST_CHECK(reflect::hashOf(q.settings, reflect::kFieldView) == reflect::hashOf(other, reflect::kFieldView));
}

// 같은 조작을 하면서 매 틱 해시를 남긴다. 30틱째에 바디 하나를 지우고 새 바디를 만든다 (복원이 바디 목록 변화도 되돌리는지)
std::vector<u64> runPhysics(physics::PhysicsWorld& w, std::vector<physics::BodyId>& bodies, u32 ticks, u32 startTick) {
    std::vector<u64> hashes;
    for (u32 t = 0; t < ticks; ++t) {
        const u32 tick = startTick + t;
        if (tick == 30) {
            w.destroy(bodies[3]);
            physics::BodyDesc d;
            d.position = {0.5f, 12.0f, 0.25f};
            bodies[3] = w.createSphere(0.4f, d);
        }
        if (tick % 7 == 0) w.addLinearVelocity(bodies[tick % bodies.size()], {1.0f, 3.0f, -0.5f});
        w.step(1.0f / 30.0f, 2);
        Hasher h;
        w.hashState(h);
        for (physics::BodyId id : bodies) h.add(id);
        hashes.push_back(h.value);
    }
    return hashes;
}

TEST_CASE(physics_snapshot) {
    physics::PhysicsWorld w;
    w.init(1024, 2);
    w.setGravity({0.0f, -20.0f, 0.0f});
    physics::BodyDesc ground;
    ground.motion = physics::Motion::Static;
    ground.position = {0.0f, -0.5f, 0.0f};
    w.createBox({20.0f, 0.5f, 20.0f}, ground);
    Pcg32 rng(7);
    std::vector<physics::BodyId> bodies;
    for (int i = 0; i < 60; ++i) {
        physics::BodyDesc d;
        d.position = {rng.range(-3.0f, 3.0f), rng.range(1.0f, 10.0f), rng.range(-3.0f, 3.0f)};
        bodies.push_back(w.createBox({0.3f, 0.3f, 0.3f}, d));
    }
    runPhysics(w, bodies, 20, 0);

    physics::PhysicsSnapshot snap;
    w.save(snap);
    const std::vector<physics::BodyId> savedBodies = bodies;
    const std::vector<u64> first = runPhysics(w, bodies, 40, 20); // 30틱에 바디가 바뀐다
    const physics::BodyId newBody = bodies[3];

    TEST_CHECK(w.restore(snap));
    bodies = savedBodies;
    const std::vector<u64> second = runPhysics(w, bodies, 40, 20);
    TEST_CHECK(first == second);
    TEST_CHECK(bodies[3] == newBody); // 다시 만든 바디의 id도 같다
    TEST_CHECK(w.bodyCount() == 61);
    std::printf("  physics snapshot: %zu bytes for 61 bodies\n", snap.byteSize());
}

bool near(Vec3 a, Vec3 b) {
    return std::fabs(a.x - b.x) < 1e-4f && std::fabs(a.y - b.y) < 1e-4f && std::fabs(a.z - b.z) < 1e-4f;
}

TEST_CASE(scene_hierarchy) {
    scene::Scene s;
    s.entities.reserve(8); // add()가 돌려준 참조를 계속 쓴다
    scene::Entity& parent = s.add(scene::kTypeSolid, "parent");
    parent.position = {10.0f, 0.0f, 0.0f};
    parent.rotation = detYawQuat(kDetHalfPi); // +x → -z ... 아래에서 계산으로 확인
    parent.scale = {2.0f, 2.0f, 2.0f};
    scene::Entity& child = s.add(scene::kTypeSolid, "child");
    child.parent = parent.id;
    child.position = {1.0f, 0.0f, 0.0f};
    scene::Entity& grandchild = s.add(scene::kTypeSpawn, "grandchild");
    grandchild.parent = child.id;
    grandchild.position = {0.0f, 1.0f, 0.0f};

    const Vec3 expected = Vec3{10.0f, 0.0f, 0.0f} + rotate(s.entities[0].rotation, Vec3{2.0f, 0.0f, 0.0f});
    const scene::Transform cw = scene::worldTransform(s, s.entities[1]);
    TEST_CHECK(near(cw.position, expected) && near(cw.scale, {2.0f, 2.0f, 2.0f}));
    TEST_CHECK(near(scene::worldTransform(s, s.entities[2]).position, expected + Vec3{0.0f, 2.0f, 0.0f}));

    scene::setWorldTransform(s, s.entities[1], {{0.0f, 4.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}});
    TEST_CHECK(near(scene::worldTransform(s, s.entities[1]).position, {0.0f, 4.0f, 0.0f}));

    TEST_CHECK(scene::isDescendant(s, 3, 1) && !scene::isDescendant(s, 1, 3) && scene::subtree(s, 2).size() == 2);
    const scene::Scene flat = scene::flattened(s);
    TEST_CHECK(flat.entities[2].parent == 0 &&
               near(flat.entities[2].position, scene::worldTransform(s, s.entities[2]).position));

    // 저장 → 불러오기: 부모 유지, 잘못된 부모는 최상위로
    s.entities[0].parent = 99;
    const std::string path = executableDirectory() + "\\test_hierarchy.scene";
    TEST_CHECK(scene::save(s, path));
    scene::Scene loaded;
    TEST_CHECK(scene::load(loaded, path));
    TEST_CHECK(loaded.entities.size() == 3 && loaded.entities[0].parent == 0 && loaded.entities[2].parent == 2);
    TEST_CHECK(loaded.contentHash() == [&] {
        s.entities[0].parent = 0;
        return s.contentHash();
    }());
    removeFile(path);
}

// ---------------------------------------------------------------- Simulation<World> (그림자 시뮬레이션, 되감기)

MEALKIT_STRUCT(FakeState) {
    MEALKIT_FIELD(u32, tick, 0, "틱");
    MEALKIT_FIELD(u32, sum, 0, "입력 합");
    MEALKIT_FIELD(physics::BodyId, ball, physics::kInvalidBody, "공");
};

int g_globalCounter = 0; // 일부러 만든 전역 상태 (그림자 시뮬레이션이 잡아야 한다)

struct FakeEvent {
    u32 tick = 0;
};

struct FakeWorld {
    FakeState state;
    physics::PhysicsWorld physics;
    sim::TickEvents<FakeEvent, 4> events;
    bool useGlobal = false; // 전역 상태를 섞는다 → 결정론 위반
    u32 undeclared = 0;     // 선언하지 않은 상태 → 되감기 검증이 잡아야 한다
    bool useUndeclared = false;

    void reset(u32 threads) {
        physics.init(256, threads);
        physics.setGravity({0.0f, -10.0f, 0.0f});
        physics::BodyDesc ground;
        ground.motion = physics::Motion::Static;
        physics.createBox({10.0f, 0.5f, 10.0f}, ground);
        physics::BodyDesc d;
        d.position = {0.0f, 5.0f, 0.0f};
        state = FakeState{};
        state.ball = physics.createSphere(0.5f, d);
    }
    void step(const net::TickInputs& in) {
        events.clear();
        state.sum += in.players[0].bytes[0];
        if (useGlobal) state.sum += static_cast<u32>(g_globalCounter++);
        if (useUndeclared) state.sum += undeclared++;
        if (state.tick % 20 == 0) physics.addLinearVelocity(state.ball, {0.5f, 6.0f, 0.0f});
        physics.step(1.0f / 30.0f, 1);
        events.push({state.tick});
        ++state.tick;
    }
    u64 hash() const {
        Hasher h;
        reflect::hashStruct(reflect::typeOf<FakeState>(), &state, h);
        physics.hashState(h);
        return h.value;
    }
};

// 오프라인 세션으로 ticks틱을 돌린다
void runFake(game::Simulation<FakeWorld>& sim, net::LockstepSession& session, u32 ticks) {
    auto sample = [](u32 tick) {
        net::PlayerInput pi;
        pi.bytes[0] = static_cast<u8>(tick * 7 + 3);
        return pi;
    };
    u32 events = 0;
    const u32 before = sim.nextTick();
    auto onTick = [&](const FakeWorld& w) { events += w.events.size(); };
    for (u32 i = 0; i < ticks * 4 && sim.nextTick() < ticks; ++i) sim.update(session, 1.0 / 30.0, sample, onTick);
    TEST_CHECK(events == sim.nextTick() - before); // 틱마다 이벤트가 화면 쪽으로 넘어간다
}

TEST_CASE(simulation_shadow_and_rewind) {
    { // 정상: 그림자 시뮬레이션이 조용하고, 되감아 다시 돌린 결과가 기록과 같다
        net::LockstepSession session;
        session.startOffline(1, 0);
        game::PlayOptions options;
        options.shadow = true;
        options.record = true;
        game::Simulation<FakeWorld> sim;
        sim.start(options, 1.0f / 30.0f, [](FakeWorld& w, u32 threads) { w.reset(threads); });
        runFake(sim, session, 400);
        TEST_CHECK(sim.nextTick() == 400 && !sim.shadow().desynced);
        const u64 liveHash = sim.world().hash();
        const u32 liveSum = sim.world().state.sum;

        sim.setPaused(true);
        const game::Timeline t = sim.timeline();
        TEST_CHECK(t.recording && t.first == 300 - 2 * 150 && t.live == 400); // 0, 150, 300틱 스냅샷
        TEST_CHECK(sim.seek(37) && sim.world().state.tick == 37 && sim.timeline().viewingPast);
        TEST_CHECK(sim.seek(333) && sim.world().state.tick == 333);
        sim.stepOnce();
        TEST_CHECK(sim.world().state.tick == 334);
        TEST_CHECK(sim.replayError().empty());
        sim.setPaused(false); // 현재로 돌아온다
        TEST_CHECK(sim.world().hash() == liveHash && sim.world().state.sum == liveSum && !sim.timeline().viewingPast);

        sim.setPaused(true); // 현재 틱에서 한 틱 진행
        sim.stepOnce();
        runFake(sim, session, 401);
        TEST_CHECK(sim.nextTick() == 401);
        sim.setPaused(false);
        runFake(sim, session, 420);
        TEST_CHECK(sim.nextTick() == 420 && !sim.shadow().desynced);
    }
    { // 전역 상태 → 그림자 시뮬레이션이 첫 틱에 잡고 멈춘다
        net::LockstepSession session;
        session.startOffline(1, 0);
        game::PlayOptions options;
        options.shadow = true;
        game::Simulation<FakeWorld> sim;
        sim.start(options, 1.0f / 30.0f, [](FakeWorld& w, u32 threads) {
            w.reset(threads);
            w.useGlobal = true;
        });
        runFake(sim, session, 10);
        TEST_CHECK(sim.shadow().desynced && sim.shadow().tick == 0 && sim.timeline().paused);
        TEST_CHECK(!sim.shadow().diff.empty() && sim.shadow().diff[0].rfind("sum:", 0) == 0);
    }
    { // 선언하지 않은 상태 → 되감기 검증이 잡는다
        net::LockstepSession session;
        session.startOffline(1, 0);
        game::PlayOptions options;
        options.record = true;
        game::Simulation<FakeWorld> sim;
        sim.start(options, 1.0f / 30.0f, [](FakeWorld& w, u32 threads) {
            w.reset(threads);
            w.useUndeclared = true;
        });
        runFake(sim, session, 60);
        sim.setPaused(true);
        sim.seek(30);
        TEST_CHECK(!sim.replayError().empty());
    }
}

} // namespace

int main() {
    log::setDialogs(false);
    return test::runAll();
}
