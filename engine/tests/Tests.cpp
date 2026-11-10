// Engine unit tests (mealkit_tests.exe). Only in engine repo builds. tools\check.ps1 runs them in Release and Editor builds.
//   mealkit_tests.exe          all tests, PASS or FAILED at the end
//   mealkit_tests.exe --only net,physics   only tests of those areas (engine folder names)
//   The "detmath-bits <hash>" line in the output is a hash of deterministic math result bits. It must match across build configurations.
#include <atomic>

#include "mealkit/core/DetMath.h"
#include "mealkit/core/Hash.h"
#include "mealkit/core/Log.h"
#include "mealkit/core/Memory.h"
#include "mealkit/core/Random.h"
#include "mealkit/asset/Animation.h"
#include "mealkit/data/Tables.h"
#include "mealkit/game/InputRecord.h"
#include "mealkit/game/Scenario.h"
#if MEALKIT_TEST_FRONT // front end (bug reports, audio): not in the Linux/macOS headless build
#include "mealkit/game/BugReport.h"
#endif
#include "mealkit/core/Zip.h"
#include "mealkit/game/Loadout.h"
#include "mealkit/game/ProjectSettings.h"
#include "mealkit/game/Save.h"
#include "mealkit/game/Simulation.h"
#include "mealkit/input/Actions.h"
#include "mealkit/jobs/Jobs.h"
#include "mealkit/loc/Strings.h"
#include "mealkit/core/Clock.h"
#include "mealkit/net/Lockstep.h"
#include "mealkit/net/Rooms.h"
#include "mealkit/platform/Socket.h"
#include "mealkit/core/Reflect.h"
#include "mealkit/core/TextFormat.h"
#if MEALKIT_TEST_FRONT // front end (bug reports, audio): not in the Linux/macOS headless build
#include "mealkit/audio/Audio.h"
#endif
#include "mealkit/physics/PhysicsWorld.h"
#include "mealkit/platform/Input.h"
#include "mealkit/platform/Pack.h"
#include "mealkit/platform/Paths.h"
#include "mealkit/scene/Scene.h"
#include "mealkit/sim/Commands.h"
#include "mealkit/sim/MatchFlow.h"
#include "mealkit/sim/Movement.h"
#include "mealkit/sim/Pool.h"
#include "mealkit/sim/TickEvents.h"
#include "mealkit/sim/TickTimer.h"
#include "mealkit/asset/AnimStates.h"
#include "mealkit/asset/Cooked.h"
#include "mealkit/asset/Cutscene.h"
#include "mealkit/render/Environment.h"
#include "mealkit/render/TextureCache.h"
#include "mealkit/render/SceneLights.h"
#include "mealkit/render/ModelDraw.h" // asset::ModelCache + its drawing
#include "mealkit/asset/Scatter.h"
#include "mealkit/asset/Terrain.h"
#include "mealkit/render/Culling.h"
#include "mealkit/editor/FuzzyMatch.h" // standard library only, so these two tests also run in the headless build
#include "mealkit/editor/UndoStack.h"
#if MEALKIT_TEST_UI
#include "mealkit/camera/Brain.h"
#include "mealkit/game/FrameStats.h"
#include "mealkit/live/Server.h"
#include "mealkit/settings/Settings.h"
#include "mealkit/ui/Document.h"
#include "mealkit/ui/Ui.h"
#endif

#include "TestRegistry.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

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

TEST_CASE(sim, pool) {
    using sim::Id;
    using sim::Pool;
    Pool<Item> p;
    p.init(3);
    const Id<Item> a = p.create({1});
    const Id<Item> b = p.create({2});
    TEST_CHECK(a.slot == 0 && b.slot == 1);
    TEST_CHECK(p.get(a) && p.get(a)->value == 1); // get works even on the creation tick
    TEST_CHECK(countIter(p) == 0);                // not in iteration on the creation tick
    p.endTick();
    TEST_CHECK(countIter(p) == 2);

    u32 seen = 0;
    for (auto [id, it] : p) { // created during iteration: not in this tick's iteration
        ++seen;
        if (it.value == 1) p.create({3});
    }
    TEST_CHECK(seen == 2);
    TEST_CHECK(p.count() == 3);

    p.destroy(a);
    p.destroy(a); // deleting twice is ignored
    TEST_CHECK(p.get(a) == nullptr);
    TEST_CHECK(countIter(p) == 1);
    u32 destroyed = 0;
    p.endTick([&](Item& it) {
        TEST_CHECK(it.value == 1);
        ++destroyed;
    });
    TEST_CHECK(destroyed == 1 && p.count() == 2 && countIter(p) == 2);

    const Id<Item> c = p.create({5}); // slot reuse: the old id has a different generation, so it's invalid
    TEST_CHECK(c.slot == 0 && c.generation == 2);
    TEST_CHECK(p.get(a) == nullptr && p.get(c)->value == 5);
    p.destroy(c); // delete on the creation tick
    p.endTick();
    TEST_CHECK(p.count() == 2 && p.get(c) == nullptr);

    Pool<Item> x, y; // same result regardless of deletion order
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

TEST_CASE(sim, tick_timer_and_events) {
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

TEST_CASE(sim, match_flow) {
    using sim::FlowEvent;
    using sim::FlowPhase;
    sim::MatchRules r;
    r.minPlayers = 2;
    r.countdown = 3;
    r.roundLength = 10;
    r.roundEndDelay = 2;
    r.rounds = 3;
    r.scoreLimit = 5;
    sim::MatchPlayer p[2];
    p[0].present = true;
    sim::MatchFlow f;
    TEST_CHECK(sim::startMatchFlow(f, r, 0, p) == FlowEvent::None && f.phase == FlowPhase::Waiting); // one player: wait
    p[1].present = true;
    TEST_CHECK(sim::advanceMatch(f, r, 1, p) == FlowEvent::CountdownStart);
    TEST_CHECK(sim::advanceMatch(f, r, 3, p) == FlowEvent::None);
    TEST_CHECK(sim::advanceMatch(f, r, 4, p) == FlowEvent::RoundStart && f.round == 1);
    p[1].score = 5; // score limit
    TEST_CHECK(sim::advanceMatch(f, r, 5, p) == FlowEvent::RoundEnd && f.roundWinner == 1 && f.roundWins[1] == 1);
    p[1].score = 0;
    TEST_CHECK(sim::advanceMatch(f, r, 7, p) == FlowEvent::CountdownStart);
    TEST_CHECK(sim::advanceMatch(f, r, 10, p) == FlowEvent::RoundStart && f.round == 2);
    TEST_CHECK(sim::advanceMatch(f, r, 19, p) == FlowEvent::None);
    TEST_CHECK(sim::advanceMatch(f, r, 20, p) == FlowEvent::RoundEnd && f.roundWinner == sim::kNoSide); // time up, 0:0 draw
    TEST_CHECK(sim::advanceMatch(f, r, 22, p) == FlowEvent::CountdownStart);
    TEST_CHECK(sim::advanceMatch(f, r, 25, p) == FlowEvent::RoundStart && f.round == 3);
    p[0].score = 2; // time up, player 0 ahead
    TEST_CHECK(sim::advanceMatch(f, r, 35, p) == FlowEvent::RoundEnd && f.roundWinner == 0);
    TEST_CHECK(sim::advanceMatch(f, r, 37, p) == FlowEvent::MatchEnd && f.phase == FlowPhase::Results && f.matchWinner == sim::kNoSide); // 1:1

    // Elimination with teams: slots 0,2 = side 0, slots 1,3 = side 1; best of 1, no countdown
    sim::MatchRules e;
    e.elimination = true;
    e.teamCount = 2;
    sim::MatchPlayer q[4];
    for (sim::MatchPlayer& x : q) x.present = true;
    TEST_CHECK(sim::startMatchFlow(f, e, 0, q) == FlowEvent::RoundStart);
    q[1].alive = false;
    TEST_CHECK(sim::advanceMatch(f, e, 1, q) == FlowEvent::None); // slot 3 still alive
    q[3].alive = false;
    TEST_CHECK(sim::advanceMatch(f, e, 2, q) == FlowEvent::RoundEnd && f.roundWinner == 0);
    TEST_CHECK(sim::advanceMatch(f, e, 2, q) == FlowEvent::MatchEnd && f.matchWinner == 0);

    // Co-op: everyone is side 0. The game decides a win (finishRound), time running out or everyone out is a loss
    sim::MatchRules c;
    c.coop = true;
    c.elimination = true;
    c.roundLength = 10;
    c.rounds = 3;
    for (sim::MatchPlayer& x : q) x.alive = true;
    TEST_CHECK(sim::matchSide(c, 3) == 0 && sim::matchSideCount(c) == 1);
    TEST_CHECK(sim::startMatchFlow(f, c, 0, q) == FlowEvent::RoundStart);
    q[0].alive = q[1].alive = q[2].alive = false;
    TEST_CHECK(sim::advanceMatch(f, c, 1, q) == FlowEvent::None); // slot 3 still up
    TEST_CHECK(sim::finishRound(f, c, 2, 0) == FlowEvent::RoundEnd && f.roundWinner == 0 && f.roundWins[0] == 1);
    TEST_CHECK(sim::finishRound(f, c, 2, 0) == FlowEvent::None); // only while playing
    TEST_CHECK(sim::advanceMatch(f, c, 2, q) == FlowEvent::CountdownStart); // no countdown time: the round starts on the next call
    TEST_CHECK(sim::advanceMatch(f, c, 2, q) == FlowEvent::RoundStart && f.round == 2);
    TEST_CHECK(sim::advanceMatch(f, c, 12, q) == FlowEvent::RoundEnd && f.roundWinner == sim::kNoSide); // time up: lost
    TEST_CHECK(sim::advanceMatch(f, c, 12, q) == FlowEvent::CountdownStart);
    TEST_CHECK(sim::advanceMatch(f, c, 12, q) == FlowEvent::RoundStart && f.round == 3);
    q[3].alive = false;
    TEST_CHECK(sim::advanceMatch(f, c, 13, q) == FlowEvent::RoundEnd && f.roundWinner == sim::kNoSide); // everyone out: lost
    TEST_CHECK(sim::advanceMatch(f, c, 13, q) == FlowEvent::MatchEnd && f.matchWinner == sim::kNoSide); // 1 won, 2 lost
}

TEST_CASE(core, memory) {
    using namespace mealkit;
    const mem::Tag t = mem::tag("test_memory");
    TEST_CHECK(mem::tag("test_memory") == t);
    TEST_CHECK(std::string(mem::tagName(t)) == "test_memory");
    TEST_CHECK(mem::tag("").index == 0);
    const mem::TagStats before = mem::stats(t);

    // Counted heap, also through pmr containers
    void* p = mem::allocate(t, 1000, 64);
    TEST_CHECK(reinterpret_cast<uintptr_t>(p) % 64 == 0);
    TEST_CHECK(mem::stats(t).bytes == before.bytes + 1000);
    mem::deallocate(t, p, 1000, 64);
    TEST_CHECK(mem::stats(t).bytes == before.bytes);
    TEST_CHECK(mem::stats(t).peak >= before.bytes + 1000);
    {
        std::pmr::vector<int> v{&mem::heap(t)};
        v.resize(256);
        TEST_CHECK(mem::stats(t).bytes >= before.bytes + 256 * sizeof(int));
    }
    TEST_CHECK(mem::stats(t).bytes == before.bytes);

    // Arena: aligned bumps, reset keeps pages, trim gives back all but the first, big blocks get a page of their own
    {
        mem::Arena a{t, 1024};
        void* x = a.allocate(10, 1);
        void* y = a.allocate(100, 32);
        TEST_CHECK(x != y && reinterpret_cast<uintptr_t>(y) % 32 == 0);
        void* big = a.allocate(5000, 16);
        TEST_CHECK(big != nullptr && a.reserved() >= 1024 + 5000);
        const size_t reserved = a.reserved();
        a.reset();
        TEST_CHECK(a.used() == 0 && a.reserved() == reserved);
        TEST_CHECK(a.allocate(10, 1) == x); // the first page is reused from its start
        a.trim();
        TEST_CHECK(a.reserved() == 1024);
        std::pmr::vector<int> v{&a};
        v.assign(100, 7);
        TEST_CHECK(v[99] == 7);
    }
    TEST_CHECK(mem::stats(t).bytes == before.bytes);

    // FixedPool: blocks reused through the free list, pages added as needed
    {
        mem::FixedPool pool{t, 24, 8, 4};
        void* blocks[6];
        for (void*& b : blocks) b = pool.allocate();
        TEST_CHECK(pool.count() == 6 && pool.capacity() == 8);
        pool.free(blocks[2]);
        TEST_CHECK(pool.allocate() == blocks[2]);
        for (void* b : blocks) pool.free(b);
        TEST_CHECK(pool.count() == 0);
        struct Item { int a; double b; };
        Item* item = pool.create<Item>(Item{3, 2.5});
        TEST_CHECK(item->a == 3 && item->b == 2.5);
        pool.destroy(item);
    }
    TEST_CHECK(mem::stats(t).bytes == before.bytes);

    // Frame arena is empty after endFrame
    (void)mem::frameArena().allocate(128, 16);
    TEST_CHECK(mem::frameArena().used() >= 128);
    mem::endFrame();
    TEST_CHECK(mem::frameArena().used() == 0);
}

TEST_CASE(core, det_math) {
    f32 worstSin = 0.0f, worstCos = 0.0f, worstAtan = 0.0f;
    Hasher bits;
    for (i32 i = -200000; i <= 200000; ++i) {
        const f32 x = static_cast<f32>(i) * 0.0005f; // -100..100 radians
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
    // Simulation pose sampling (asset::samplePoseTick): tick → frame interpolation gives the same bits on all builds/platforms
    asset::Skeleton skeleton;
    skeleton.joints.resize(3);
    skeleton.joints[1].parent = 0;
    skeleton.joints[2].parent = 1;
    asset::AnimationClip clip;
    clip.frames = 31;
    clip.duration = 1.0f;
    for (u32 f = 0; f < clip.frames; ++f)
        for (u32 j = 0; j < 3; ++j) {
            const f32 a = static_cast<f32>(f * 3 + j) * 0.21f;
            clip.poses.push_back({{detSin(a), 0.5f * detCos(a), 0.1f * static_cast<f32>(j)}, detQuatFromAxisAngle({0.3f, 1.0f, 0.2f}, a), {1, 1, 1}});
        }
    std::vector<asset::JointPose> pose;
    for (const u32 rate : {30u, 60u})
        for (u32 tick = 0; tick < 200; ++tick) {
            asset::samplePoseTick(skeleton, clip, tick, rate, true, pose);
            for (const asset::JointPose& jp : pose) {
                bits.add(jp.translation);
                bits.add(jp.rotation);
            }
        }
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
    u32 notReflected = 99; // undeclared fields are excluded from hashing/saving
};

MEALKIT_OBJECT(TestPad) {
    static constexpr const char* kObjectLabel = "시험 발판";
    static constexpr u32 kObjectFlags = reflect::kObjectMarker;
    MEALKIT_FIELD(f32, power, 12.0f, "힘");
};

TEST_CASE(core, reflect) {
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
    TEST_CHECK(std::string(reflect::typeOf<Inner>().fields[1].label) == "v"); // an empty label becomes the name

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

    // Snapshots: save → change → restore
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

TEST_CASE(core, reflect_string) {
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
    // Length goes into the hash too (so "ab"+"c" and "a"+"bc" differ)
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

TEST_CASE(core, text_floats) {
    char text[96];
    text::writeFloat(text, sizeof(text), 0.4f);
    TEST_CHECK(std::strcmp(text, "0.4") == 0);
    const f32 v[4] = {0.28f, -20.0f, -0.0f, 1e-5f};
    text::writeFloats(text, sizeof(text), v, 4);
    TEST_CHECK(std::strcmp(text, "0.28 -20 0 1e-05") == 0);
    // Rereading gives identical bits
    Pcg32 rng(7);
    for (int i = 0; i < 2000; ++i) {
        const u32 bits = rng.next();
        f32 x;
        std::memcpy(&x, &bits, 4);
        if (!std::isfinite(x)) continue;
        text::writeFloat(text, sizeof(text), x);
        const f32 y = std::strtof(text, nullptr);
        TEST_CHECK(std::memcmp(&x, &y, 4) == 0 || (x == 0.0f && y == 0.0f));
    }
}

MEALKIT_STRUCT(Timed) {
    MEALKIT_FIELD(Ticks, cooldown, Ticks::seconds(0.5f), "재사용 대기");
    MEALKIT_FIELD(Color, tint, (Color::rgb(255, 128, 0)), "색");
};

TEST_CASE(core, reflect_ticks_and_color) {
    const reflect::TypeInfo& t = reflect::typeOf<Timed>();
    TEST_CHECK(t.fields[0].value->kind == reflect::Kind::Ticks && t.fields[1].value->kind == reflect::Kind::Color);
    TEST_CHECK(reflect::tickRate() == 30);
    Timed a;
    TEST_CHECK(a.cooldown.value == 15 && a.tint.value == Color::rgb(255, 128, 0).value);
    // Seconds → ticks rounding, ticks → seconds → ticks gives the same value
    for (const char* text : {"1.33333", "1.3333", "0.666667", "0.0166", "2", "-1", "0"}) {
        Timed b;
        TEST_CHECK(reflect::setFromText(t, &b, "cooldown", text));
        reflect::TextFields fields;
        reflect::toText(t, &b, fields);
        Timed c;
        reflect::fromText(t, &c, fields);
        TEST_CHECK(c.cooldown.value == b.cooldown.value);
    }
    TEST_CHECK(reflect::setFromText(t, &a, "cooldown", "1.33333") && a.cooldown.value == 40);
    TEST_CHECK(reflect::setFromText(t, &a, "cooldown", "0.0166") && a.cooldown.value == 0);
    TEST_CHECK(reflect::setFromText(t, &a, "cooldown", "-3") && a.cooldown.value == 0);
    TEST_CHECK(!reflect::setFromText(t, &a, "cooldown", "abc"));
    // Colors: #RRGGBB, #RRGGBBAA
    TEST_CHECK(reflect::setFromText(t, &a, "tint", "#102030") && a.tint.value == Color::rgb(0x10, 0x20, 0x30).value);
    TEST_CHECK(reflect::formatScalar(*t.fields[1].value, &a.tint) == "#102030");
    TEST_CHECK(reflect::setFromText(t, &a, "tint", "#10203040") && a.tint.value == Color::rgb(0x10, 0x20, 0x30, 0x40).value);
    TEST_CHECK(reflect::formatScalar(*t.fields[1].value, &a.tint) == "#10203040");
    TEST_CHECK(!reflect::setFromText(t, &a, "tint", "102030") && !reflect::setFromText(t, &a, "tint", "#12345"));
    TEST_CHECK(!reflect::setFromText(t, &a, "tint", "#12345G"));
}

MEALKIT_ENUM(TestShape, u8, Box, Ball, Capsule);

MEALKIT_STRUCT(WithEnum) {
    MEALKIT_FIELD(TestShape, shape, TestShape::Ball, "모양");
    MEALKIT_FIELD(u8, plain, 3, "그냥 수");
};

TEST_CASE(core, reflect_enum) {
    const reflect::TypeInfo& t = reflect::typeOf<WithEnum>();
    TEST_CHECK(t.fields[0].value->enumNames.size() == 3 && t.fields[0].value->enumNames[2] == "Capsule");
    TEST_CHECK(t.fields[1].value->enumNames.empty());
    TEST_CHECK(reflect::enumName(TestShape::Capsule) == "Capsule");
    WithEnum a;
    reflect::TextFields text;
    reflect::toText(t, &a, text);
    TEST_CHECK(text[0].second == "Ball" && text[1].second == "3"); // names in files
    TEST_CHECK(reflect::setFromText(t, &a, "shape", "Capsule") && a.shape == TestShape::Capsule);
    TEST_CHECK(reflect::setFromText(t, &a, "shape", "0") && a.shape == TestShape::Box); // old numbers are read too
    TEST_CHECK(!reflect::setFromText(t, &a, "shape", "Cone"));
    WithEnum b;
    b.shape = TestShape::Box;
    TEST_CHECK(reflect::hashOf(a) == reflect::hashOf(b)); // the hash uses the integer as is
}

TEST_CASE(input, input_actions) {
    // assets/input.bindings in a temp project folder
    const std::string oldRoot = projectRoot();
    const std::string dir = executableDirectory() + "/test_input_actions";
    createDirectories(dir + "/assets");
    TEST_CHECK(writeTextFile(dir + "/assets/input.bindings",
                             "row move\nkind Axis2\nkeys W S A D\npad LeftStick\n"
                             "row jump\nkeys Space\npad A\n"
                             "row zoom\nkind Delta\nkeys Q E\nmouse Wheel\npad DPadUp -DPadDown\nrate 10\n"));
    removeFile(dir + "/user.bindings");
    setProjectRoot(dir);
    input::Actions actions;
    actions.setUserFile(dir + "/user.bindings");
    actions.load();
    TEST_CHECK(actions.count() == 3);

    Input in;
    PadState pad;
    in.setKey('W', true);
    in.setKey('D', true);
    actions.update(in, pad, true, true, 0.1);
    const Vec2 m = actions.axis2("move");
    TEST_CHECK(std::fabs(length(m) - 1.0f) < 1e-5f && m.x > 0.7f && m.y > 0.7f); // diagonals have length 1 too
    actions.update(in, pad, false, true, 0.1); // released while not receiving the keyboard
    TEST_CHECK(length(actions.axis2("move")) == 0.0f);

    in.beginFrame();
    in.setKey(key::Space, true);
    actions.update(in, pad, true, true, 0.1);
    TEST_CHECK(actions.pressed("jump") && actions.down("jump"));
    in.beginFrame();
    actions.update(in, pad, true, true, 0.1);
    TEST_CHECK(!actions.pressed("jump") && actions.down("jump"));
    in.beginFrame();
    in.setKey(key::Space, false);
    actions.update(in, pad, true, true, 0.1);
    TEST_CHECK(actions.released("jump") && !actions.down("jump"));

    in.beginFrame();
    in.setKey('E', true);
    in.addWheel(1.0f);
    actions.update(in, pad, true, true, 0.5);
    TEST_CHECK(std::fabs(actions.axis("zoom") - 6.0f) < 1e-5f); // keys are rate × seconds, the wheel notches as is

    // Gamepad: 0 inside the deadzone, rescaled to 0..1 outside
    in.beginFrame();
    in.clear();
    pad.connected = true;
    pad.leftX = 0.1f;
    actions.update(in, pad, true, true, 0.1);
    TEST_CHECK(length(actions.axis2("move")) == 0.0f);
    pad.leftX = 0.6f;
    pad.buttons = pad::A;
    actions.update(in, pad, true, true, 0.1);
    TEST_CHECK(std::fabs(actions.axis2("move").x - 0.5f) < 1e-5f && actions.pressed("jump"));
    TEST_CHECK(actions.lastDevice() == input::Device::Gamepad);
    pad.buttons = pad::DPadDown; // "-name" is the negative side
    actions.update(in, pad, true, true, 0.5);
    TEST_CHECK(std::fabs(actions.axis("zoom") + 5.0f) < 1e-5f);
    pad = {};

    // Keys rebound by the player: only changed rows are saved to the user file, and rereading overlays them on the project values
    input::Action jump = actions.binding(1);
    jump.keys = "J";
    TEST_CHECK(actions.setBinding("jump", jump) && actions.saveUser());
    input::Actions again;
    again.setUserFile(dir + "/user.bindings");
    again.load();
    TEST_CHECK(again.binding(1).keys == "J" && again.binding(1).pad == "A" && again.binding(0).keys == "W S A D");
    in.beginFrame();
    in.setKey('J', true);
    again.update(in, pad, true, true, 0.1);
    TEST_CHECK(again.down("jump"));
    again.resetBinding("jump");
    TEST_CHECK(again.saveUser() && !fileExists(dir + "/user.bindings")); // all defaults deletes the file

    TEST_CHECK(input::keyFromName("Space") == key::Space && input::keyFromName("q") == 'Q' && input::keyFromName("Nope") == 0);
    TEST_CHECK(std::string(input::keyName('Q')) == "Q" && std::string(input::keyName(key::Enter)) == "Enter");
    setProjectRoot(oldRoot);
}

MEALKIT_STRUCT(TestKind) {
    MEALKIT_FIELD(f32, weight, 1.0f, "무게");
    MEALKIT_FIELD(std::string, title, "", "이름");
};

MEALKIT_STRUCT(TestItem) {
    MEALKIT_FIELD(u32, count, 1, "개수");
    MEALKIT_FIELD(RowRef<TestKind>, kind, "plain", "종류");
    MEALKIT_FIELD(Ticks, delay, Ticks::seconds(0.2f), "대기");
};

static_assert(reflect::hasStringField<TestItem>()); // RowRef contains an id string, so it isn't put in simulation state

// Asset path → project-relative file path. Source module assets are under that module's assets/ (docs/MODULES.md)
TEST_CASE(data, asset_file_path) {
    TEST_CHECK(assetFilePath("ui/hud.ui") == "assets/ui/hud.ui");
    TEST_CHECK(assetFilePath("modules/health/ui/bar.ui") == "modules/health/assets/ui/bar.ui");
    TEST_CHECK(assetFilePath("modules/inventory/tables/items.table") == "modules/inventory/assets/tables/items.table");
    TEST_CHECK(assetFilePath("modules/") == "assets/modules/");
    TEST_CHECK(assetFilePath("modules//x.ui") == "assets/modules//x.ui");
}

TEST_CASE(data, tables) {
    const std::string dir = executableDirectory();
    // Files: writing and reading gives the same. Duplicate/invalid ids are dropped
    data::TableText text;
    text.ids = {"plain", "heavy"};
    text.rows = {{{"weight", "1"}, {"title", "Plain, \"basic\""}}, {{"weight", "4.5"}, {"title", "Heavy"}}};
    TEST_CHECK(data::writeTableText(dir + "/test_kinds.table", text, "TestKind"));
    std::string body;
    TEST_CHECK(readTextFile(dir + "/test_kinds.table", body));
    TEST_CHECK(writeTextFile(dir + "/test_kinds.table", body + "\nrow heavy\nweight 9\nrow bad id\nweight 2\n"));
    sim::Table<TestKind> kinds;
    TEST_CHECK(data::loadTable(kinds, "../build/does-not-exist.table") == false && kinds.size() == 0);
    data::TableData data;
    std::string warnings;
    TEST_CHECK(data::loadTableData(reflect::typeOf<TestKind>(), dir + "/test_kinds.table", data, &warnings));
    TEST_CHECK(data.ids.size() == 2 && data.ids[1] == "heavy" && !warnings.empty());
    TEST_CHECK(static_cast<const TestKind*>(data.rows[1].data())->weight == 4.5f);
    TEST_CHECK(static_cast<const TestKind*>(data.rows[0].data())->title == "Plain, \"basic\"");

    // CSV: exporting and importing gives the same (cells with commas/quotes)
    TEST_CHECK(data::exportTableCsv(data, dir + "/test_kinds.csv"));
    data::TableData back;
    back.rowType = &reflect::typeOf<TestKind>();
    std::string error;
    TEST_CHECK(data::importTableCsv(back, dir + "/test_kinds.csv", &error));
    TEST_CHECK(back.ids == data.ids);
    for (size_t i = 0; i < back.rows.size(); ++i)
        TEST_CHECK(reflect::hashOf(*static_cast<const TestKind*>(back.rows[i].data())) ==
                   reflect::hashOf(*static_cast<const TestKind*>(data.rows[i].data())));
    // Matches by name even with different column order or unknown columns
    TEST_CHECK(writeTextFile(dir + "/test_kinds2.csv", "\xEF\xBB\xBFid,title,extra,weight\r\nx,\"multi\nline\",1,7\r\n"));
    TEST_CHECK(data::importTableCsv(back, dir + "/test_kinds2.csv", &error) && back.ids.size() == 1);
    TEST_CHECK(static_cast<const TestKind*>(back.rows[0].data())->weight == 7.0f);
    TEST_CHECK(static_cast<const TestKind*>(back.rows[0].data())->title == "multi\nline");
    TEST_CHECK(!data::importTableCsv(back, dir + "/test_kinds.table", &error)); // the first cell isn't id

    // Row references: id → index, missing ids into the list
    sim::Table<TestKind> table;
    table.ids = {"plain", "heavy"};
    table.rows.resize(2);
    TestItem item;
    std::vector<std::string> missing;
    data::resolveRows(item, {data::tableIndex(table)}, &missing);
    TEST_CHECK(item.kind.index == 0 && missing.empty() && table.get(item.kind) == &table.rows[0]);
    TEST_CHECK(reflect::setFromText(reflect::typeOf<TestItem>(), &item, "kind", "heavy") && !item.kind.valid());
    data::resolveRows(item, {data::tableIndex(table)}, &missing);
    TEST_CHECK(item.kind.index == 1 && table.find("heavy") == 1 && table.find("nope") == kNoRow);
    item.kind.id = "nope";
    data::resolveRows(item, {data::tableIndex(table)}, &missing, "test: ");
    TEST_CHECK(!item.kind.valid() && missing.size() == 1 && missing[0].rfind("test: kind -> nope", 0) == 0);
    // Hashing by id (not by row index)
    TestItem a, b;
    b.kind.index = 5;
    TEST_CHECK(reflect::hashOf(a) == reflect::hashOf(b));
    b.kind.id = "heavy";
    TEST_CHECK(reflect::hashOf(a) != reflect::hashOf(b));
    reflect::ByteWriter w;
    reflect::saveStruct(reflect::typeOf<TestItem>(), &b, w);
    TestItem c;
    reflect::ByteReader r(w.data().data(), w.data().size());
    TEST_CHECK(reflect::loadStruct(reflect::typeOf<TestItem>(), &c, r) && c.kind.id == "heavy" && c.kind.index == 5);
}

MEALKIT_ENUM(ScenarioPhase, u8, Waiting, Playing);
MEALKIT_STRUCT(ScenarioSlot) {
    MEALKIT_FIELD(u32, falls, 0, "");
    MEALKIT_FIELD(bool, connected, false, "");
};
MEALKIT_STRUCT(ScenarioState) {
    MEALKIT_FIELD(ScenarioPhase, phase, ScenarioPhase::Waiting, "");
    MEALKIT_FIELD(f32, height, 0.0f, "");
    MEALKIT_ARRAY(ScenarioSlot, 2, slots, "");
    MEALKIT_FIELD(sim::Pool<ScenarioSlot>, pool, {}, "");
};

TEST_CASE(game, scenario) {
    const std::string path = executableDirectory() + "/test.scenario";
    TEST_CHECK(writeTextFile(path, "# comment\nseed 9\ntick 1 phase == Playing\ntick 1 slots[1].falls == 2\n"
                                   "tick 1 height < 1.5\ntick 1 pool[1].falls != 0\ntick 1 pool[0].falls == 3\n"
                                   "tick 5 phase == Playing\n"));
    game::Scenario sc;
    std::string error;
    TEST_CHECK(game::loadScenario(path, sc, error) && sc.seed == 9 && sc.checks.size() == 6);
    ScenarioState state;
    state.phase = ScenarioPhase::Playing;
    state.height = 1.25f;
    state.slots[1].falls = 2;
    state.pool.init(4);
    state.pool.create();
    ScenarioSlot four;
    four.falls = 4;
    state.pool.create(four);
    game::ScenarioRun run(sc, reflect::typeOf<ScenarioState>(), false);
    TEST_CHECK(run.lastCheckTick() == 5);
    run.afterTick(1, &state);
    // pool[0].falls == 3 fails (0) and tick 5 is never reached
    TEST_CHECK(run.finish(1) == game::kScenarioFailedExit);
    game::ScenarioRun update(sc, reflect::typeOf<ScenarioState>(), true);
    for (u32 t = 0; t <= 5; ++t) update.afterTick(t, &state);
    TEST_CHECK(update.finish(5) == 0);
    TEST_CHECK(game::loadScenario(path, sc, error) && sc.checks[4].expected == "0");
    TEST_CHECK(writeTextFile(path, "tick 1 phase ~ Playing\n") && !game::loadScenario(path, sc, error));
    TEST_CHECK(writeTextFile(path, "seed 1\n") && !game::loadScenario(path, sc, error)); // no checks
    // ...accepted with --update-expected, which writes one == check per top-level scalar of the last tick
    TEST_CHECK(game::loadScenario(path, sc, error, true) && sc.checks.empty());
    game::ScenarioRun fill(sc, reflect::typeOf<ScenarioState>(), true);
    fill.afterTick(0, &state);
    fill.afterTick(1, &state);
    TEST_CHECK(fill.finish(1) == 0);
    TEST_CHECK(game::loadScenario(path, sc, error) && sc.checks.size() == 2 && sc.checks[0].tick == 1 &&
               sc.checks[0].path == "phase" && sc.checks[0].expected == "Playing" && sc.checks[1].path == "height");
    removeFile(path);
}

TEST_CASE(game, input_record) {
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
        for (u32 p = 0; p < net::kMaxPlayers; ++p) // players beyond the count aren't recorded (read as 0)
            for (u32 b = 0; b < net::kInputBytes; ++b)
                same = same && rec.inputs[t].players[p].bytes[b] == (p < 3 ? inputs[t].players[p].bytes[b] : 0);
    }
    TEST_CHECK(same);
    removeFile(path);
}

TEST_CASE(game, replay_files) {
    // Per match current.rec → keepReplay keeps it under a date name and keeps only keep of them. Recordings without ticks are deleted
    const std::string user = executableDirectory() + "/test_replays";
    for (const std::string& path : game::listReplays(user)) removeFile(path);
    const game::MatchInfo match{7, 1, 0, {{}}};
    const std::string current = game::replayRecordingPath(user);
    TEST_CHECK(game::replayRecordingPath("").empty());
    for (u32 i = 0; i < 3; ++i) {
        game::InputRecordWriter w;
        TEST_CHECK(w.open(current, match, 1));
        w.write(net::TickInputs{}, i);
        w.close();
        TEST_CHECK(!game::keepReplay(user, 2).empty() && !fileExists(current));
    }
    std::vector<std::string> kept = game::listReplays(user);
    TEST_CHECK(kept.size() == 2);
    game::InputRecording rec;
    std::string error;
    TEST_CHECK(!kept.empty() && game::loadInputRecording(kept[0], rec, error) && rec.hashes.size() == 1 &&
               rec.hashes[0] == 2); // newest first
    {
        game::InputRecordWriter w;
        TEST_CHECK(w.open(current, match, 1));
    }
    TEST_CHECK(game::keepReplay(user, 2).empty() && !fileExists(current) && game::listReplays(user).size() == 2);
    for (const std::string& path : game::listReplays(user)) removeFile(path);
}

TEST_CASE(core, zip_files) {
    const u8 digits[] = {'1', '2', '3', '4', '5', '6', '7', '8', '9'};
    TEST_CHECK(crc32(digits, sizeof(digits)) == 0xCBF43926u);
    const std::string path = executableDirectory() + "/test_zip.zip";
    const std::vector<ZipEntry> in = {{"a.txt", {'h', 'i'}}, {"dir/empty.bin", {}}};
    TEST_CHECK(writeZip(path, in));
    std::vector<ZipEntry> out;
    std::string error;
    TEST_CHECK(readZip(path, out, error) && out.size() == 2 && out[0].name == "a.txt" && out[0].data == in[0].data &&
               out[1].name == "dir/empty.bin" && out[1].data.empty());
    removeFile(path);
    TEST_CHECK(!readZip(path, out, error));
}

TEST_CASE(platform, pack_files) {
    // Writer → mount: paths under the pack's folder read from the pack, findAsset sees only the pack, unmount restores the disk
    const std::string dir = executableDirectory() + "/test_pack";
    createDirectories(dir);
    PackWriter w;
    const std::string big(5000, 'x');
    w.add("assets/ui/a.ui", "hello");
    w.add("assets/ui/deep/b.txt", big);           // compresses
    w.add("assets/sounds/c.wav", "RIFF", false); // stored
    w.add("mealkit.project", "name packed\n");
    w.add("assets/ui/a.ui", "hello again");       // same path: last wins
    TEST_CHECK(w.count() == 4);
    const u64 hash = w.contentHash();
    std::string error;
    TEST_CHECK(w.write(dir + "/game.pak", error));
    TEST_CHECK(!mountPack(dir + "/missing.pak", error) && !packsMounted());
    TEST_CHECK(mountPack(dir + "/game.pak", error) && packsMounted() && mountedPackHash() == hash);
    std::string text;
    TEST_CHECK(readTextFile(dir + "/assets/ui/a.ui", text) && text == "hello again");
    TEST_CHECK(readTextFile(dir + "\\assets\\ui\\deep\\b.txt", text) && text == big);
    TEST_CHECK(fileExists(dir + "/assets/sounds/c.wav") && directoryExists(dir + "/assets/ui/deep") && !fileExists(dir + "/assets/x"));
    const std::string root = packRoot(); // dir with '/' separators
    TEST_CHECK(findAssetFile("ui/a.ui") == root + "/assets/ui/a.ui" && findAsset("assets/nope.ui").empty());
    TEST_CHECK(findAsset(game::kProjectFileName) == root + "/mealkit.project");
    TEST_CHECK((listFiles(dir + "/assets/ui") == std::vector<std::string>{"a.ui"}) &&
               (listFiles(dir + "/assets/ui/deep", ".txt") == std::vector<std::string>{"b.txt"}) &&
               (listDirectories(dir + "/assets") == std::vector<std::string>{"sounds", "ui"}));
    TEST_CHECK(fileWriteTime(dir + "/assets/ui/a.ui") == fileWriteTime(dir + "/game.pak") && fileWriteTime(dir + "/assets/ui/a.ui") != 0);
    if (FILE* f = openFile(dir + "/assets/sounds/c.wav", "rb")) {
        char buf[8] = {};
        TEST_CHECK(fread(buf, 1, sizeof(buf), f) == 4 && std::string(buf) == "RIFF");
        fclose(f);
    } else TEST_CHECK(false);
    unmountPacks();
    TEST_CHECK(!packsMounted() && !fileExists(dir + "/assets/ui/a.ui"));
    removeFile(dir + "/game.pak");
}

#if MEALKIT_ASSET_IMPORT
TEST_CASE(asset, cooked_assets) {
    // Import → cooked bytes → read back gives the same model (shipping reads only this), and the loaded model matches the source path
    asset::CookedModel cooked;
    std::string error;
    const std::string file = asset::ModelCache::resolve("models/characters/rig_figure.glb");
    TEST_CHECK(asset::importModel(file, cooked, nullptr, error));
    const std::string bytes = asset::writeCookedModel(cooked);
    asset::CookedModel back;
    TEST_CHECK(asset::readCookedModel(bytes, back, error));
    TEST_CHECK(back.fileHash == cooked.fileHash && back.hullPoints.size() == cooked.hullPoints.size() &&
               std::memcmp(back.hullPoints.data(), cooked.hullPoints.data(), back.hullPoints.size() * sizeof(Vec3)) == 0);
    TEST_CHECK(back.skeleton.joints.size() == 8 && back.skeleton.joints[0].name == "root" && back.clips.size() == 3 &&
               back.clips[0].poses.size() == cooked.clips[0].poses.size() && back.lods.size() == cooked.lods.size() &&
               back.lods[0].indices == cooked.lods[0].indices && back.lods[0].skin.size() == back.lods[0].vertices.size() &&
               back.materials.size() == cooked.materials.size());
    TEST_CHECK(!asset::readCookedModel(bytes.substr(0, bytes.size() - 1), back, error));
    TEST_CHECK(!asset::readCookedModel("junk", back, error));

    std::vector<asset::CameraClip> clips, clipsBack;
    TEST_CHECK(asset::loadCameraClips(findAssetFile("models/cutscenes/intro.glb"), clips, error) && !clips.empty());
    TEST_CHECK(asset::readCameraClips(asset::writeCameraClips(clips), clipsBack, error) && clipsBack.size() == clips.size() &&
               clipsBack[0].keys.size() == clips[0].keys.size() && clipsBack[0].duration == clips[0].duration);

    // A cooked file next to a missing source loads like the source (pack files put only cooked files in)
    const std::string dir = executableDirectory() + "/test_cooked";
    createDirectories(dir + "/assets/models");
    PackWriter w;
    w.add("assets/models/rig.glb" + std::string(asset::kCookedModelSuffix), bytes);
    TEST_CHECK(w.write(dir + "/c.pak", error) && mountPack(dir + "/c.pak", error));
    {
        asset::ModelCache models;
        const asset::Model* m = models.get("models/rig.glb");
        TEST_CHECK(m && m->skinned() && m->bones == 8 && m->fileHash == cooked.fileHash && m->lodCount == cooked.lods.size());
    }
    unmountPacks();
    removeFile(dir + "/c.pak");
}
#endif

#if MEALKIT_TEST_FRONT // front end (bug reports, audio): not in the Linux/macOS headless build
TEST_CASE(game, bug_report) {
    // Bundle = input recording + info + log (screenshot omitted if absent). The editor extracts replay.rec and plays it
    const std::string user = executableDirectory() + "/test_bugreports";
    const std::string rec = user + "/current.rec";
    createDirectories(user);
    {
        game::InputRecordWriter w;
        TEST_CHECK(w.open(rec, game::MatchInfo{3, 1, 0, {{}}}, 9));
        w.write(net::TickInputs{}, 42);
    }
    game::BugReportInfo info;
    info.tick = 1;
    info.note = "fell through\nthe floor";
    const std::string zip = game::writeBugReport(user, info, rec, user + "/missing.png");
    TEST_CHECK(!zip.empty() && fileExists(zip));
    std::string extracted, text, error;
    TEST_CHECK(game::extractBugReport(zip, user + "/out", extracted, text, error));
    TEST_CHECK(text.find("note = fell through the floor\n")!= std::string::npos);
    game::InputRecording loaded;
    TEST_CHECK(game::loadInputRecording(extracted, loaded, error) && loaded.hashes.size() == 1 && loaded.hashes[0] == 42 &&
               loaded.contentHash == 9);
    removeFile(zip);
    removeFile(extracted);
    removeFile(rec);
}

TEST_CASE(game, crash_report) {
    // What the crash handler leaves in crash/pending/ → the next run bundles it into crash/<time>.zip, the same shape as bug reports
    const std::string user = executableDirectory() + "/test_crash";
    const std::string pending = user + "/" + game::kCrashReportFolder + "/pending";
    const std::string rec = user + "/current.rec";
    createDirectories(pending);
    TEST_CHECK(game::collectCrashReport(user).empty()); // nothing pending
    {
        game::InputRecordWriter w;
        TEST_CHECK(w.open(rec, game::MatchInfo{3, 1, 0, {{}}}, 9));
        w.write(net::TickInputs{}, 42);
    }
    const auto write = [](const std::string& path, const std::string& text) {
        FILE* f = openFile(path, "wb");
        if (f) std::fwrite(text.data(), 1, text.size(), f), std::fclose(f);
    };
    write(pending + "/crash.txt", "exception = c0000005\n");
    write(pending + "/info.txt", "# MealKit crash report\ntick = 1\n");
    write(pending + "/recording.txt", rec);
    const std::string zip = game::collectCrashReport(user);
    TEST_CHECK(!zip.empty() && fileExists(zip) && !fileExists(pending + "/crash.txt"));
    std::vector<ZipEntry> entries;
    std::string error;
    TEST_CHECK(readZip(zip, entries, error) && entries.size() == 3); // info, crash.txt, replay.rec (no log/dump here)
    std::string extracted, text;
    TEST_CHECK(game::extractBugReport(zip, user + "/out", extracted, text, error) &&
               text.find("crash report") != std::string::npos);
    TEST_CHECK(game::collectCrashReport(user).empty()); // collected once
    removeFile(zip);
    removeFile(extracted);
    removeFile(rec);
}
#endif

MEALKIT_STRUCT(TestSaveV1) {
    MEALKIT_FIELD(u32, coins, 0, "코인");
    MEALKIT_FIELD(u32, oldName, 0, "예전 이름");
};

MEALKIT_STRUCT(TestSaveV2) {
    MEALKIT_FIELD(u32, coins, 0, "코인");
    MEALKIT_FIELD(u32, gems, 0, "보석");      // formerly oldName
    MEALKIT_FIELD(f32, extra, 2.5f, "새 필드"); // not in old files → default
};

TEST_CASE(game, save_files) {
    const std::string folder = executableDirectory() + "/test_saves";
    removeFile(game::savePath(folder, "progress"));
    TestSaveV1 v1;
    v1.coins = 12;
    v1.oldName = 7;
    TEST_CHECK(game::writeSave(v1, folder, "progress", 1));
    TEST_CHECK(game::writeSave(v1, folder, "progress", 1) && fileExists(game::savePath(folder, "progress") + ".bak"));
    TestSaveV2 v2;
    const game::SaveInfo info = game::readSave(v2, folder, "progress", 2, [](u32 version, reflect::TextFields& fields) {
        if (version < 2)
            for (auto& f : fields)
                if (f.first == "oldName") f.first = "gems"; // moving a renamed field
    });
    TEST_CHECK(info.found && info.version == 1 && v2.coins == 12 && v2.gems == 7 && v2.extra == 2.5f);
    TestSaveV2 none;
    TEST_CHECK(!game::readSave(none, folder, "missing", 2).found && none.coins == 0);
    TEST_CHECK(!game::writeSave(v1, "", "progress", 1)); // no saving without a user folder (automated runs)
}

TEST_CASE(loc, strings_table) {
    const std::string oldRoot = projectRoot();
    const std::string dir = executableDirectory() + "/test_strings";
    createDirectories(dir + "/assets/strings");
    TEST_CHECK(writeTextFile(dir + "/assets/strings/ko.txt",
                             "# comment\nmenu.start = 시작\nhud.score = 점수 <1>{score}</1>점\nmulti = 한 줄\\n두 줄\nbad line\n"));
    TEST_CHECK(writeTextFile(dir + "/assets/strings/en.txt", "menu.start = Start\nhud.score = Score <1>{score}</1>\n"));
    setProjectRoot(dir);

    std::vector<loc::StringEntry> entries;
    std::string errors;
    TEST_CHECK(loc::readStringsFile(dir + "/assets/strings/ko.txt", entries, &errors) && entries.size() == 3);
    TEST_CHECK(errors.find("line 5") != std::string::npos && entries[2].value == "한 줄\n두 줄" && entries[2].line == 4);

    loc::Strings s;
    s.load("en", "ko");
    TEST_CHECK(s.language() == "en" && s.get("menu.start") == "Start");
    TEST_CHECK(s.format("hud.score", {{"score", "12"}}) == "Score <1>12</1>");
    TEST_CHECK(s.get("multi") == "한 줄\n두 줄"); // if missing in en, the default language (ko)
    TEST_CHECK(s.get("no.such") == "no.such" && !s.has("no.such") && s.has("multi")); // the key name if missing
    const std::vector<std::string> langs = s.languages();
    TEST_CHECK(langs.size() == 2 && langs[0] == "en" && langs[1] == "ko");
    TEST_CHECK(loc::validStringKey("menu.start") && loc::validStringKey("Layer_1-b") && !loc::validStringKey("1abc") &&
               !loc::validStringKey("a b"));
    setProjectRoot(oldRoot);
}

#if MEALKIT_TEST_UI
MEALKIT_STRUCT(TestRowVm) {
    MEALKIT_FIELD(u32, value, 0, "값");
};

MEALKIT_VIEWMODEL(TestUiVm) {
    MEALKIT_FIELD(bool, sound, false, "소리");
    MEALKIT_FIELD(bool, showExtra, false, "더 보기");
    MEALKIT_FIELD(f32, volume, 0.5f, "볼륨");
    MEALKIT_LIST(TestRowVm, rows, 4, "줄");
};

TEST_CASE(ui, ui_layout) {
    const std::string oldRoot = projectRoot();
    const std::string dir = executableDirectory() + "/test_ui";
    createDirectories(dir + "/assets/ui");
    TEST_CHECK(writeTextFile(dir + "/assets/ui/theme.ui", "color panel #10203040\nnumber pad 20\n"));
    TEST_CHECK(writeTextFile(dir + "/assets/ui/menu.ui",
                             "board menu 1000 500\n"
                             "  navigation\n"
                             "  frame bar\n"
                             "    pos 0 0\n"
                             "    size 1000 100\n"
                             "    constraints leftright top\n"
                             "    layout row\n"
                             "    padding $pad\n"
                             "    gap 10\n"
                             "    align center\n"
                             "    fill $panel\n"
                             "    frame a\n      size 100 40\n    end\n"
                             "    frame grow\n      size fill 20\n    end\n"
                             "    frame b\n      size 50 60\n      toggle sound\n    end\n"
                             "  end\n"
                             "  frame corner\n    pos 900 450\n    size 100 50\n    constraints right bottom\n  end\n"
                             "  frame middle\n    pos 450 225\n    size 100 50\n    constraints center center\n  end\n"
                             "  frame list\n"
                             "    pos 0 200\n    size 300 hug\n    layout column\n    gap 5\n    list rows\n"
                             "    frame item\n      size 300 30\n    end\n"
                             "    frame previewOnly\n      size 300 30\n    end\n"
                             "  end\n"
                             "  frame extra\n    pos 0 400\n    size 10 10\n    visible showExtra\n  end\n"
                             "end\n"));
    TEST_CHECK(writeTextFile(dir + "/assets/ui/bad.ui", "board bad 100 100\n  frame x\n    size 10 nope\n  wobble\n  end\nend\n"));
    setProjectRoot(dir);

    ui::System sys;
    sys.init(nullptr, nullptr);
    TestUiVm vm;
    vm.rowsCount = 3;
    const ui::ScreenId menu = sys.open("ui/menu.ui");
    sys.bind(menu, vm);
    sys.layout({0, 0, 2000, 500}); // twice the width of the 1000×500 board → scale 1, the board stretches horizontally
    auto box = [&](const char* name) {
        for (const ui::NodeBox& b : sys.boxes(menu))
            if (b.name == name) return b;
        return ui::NodeBox{};
    };
    const ui::NodeBox bar = box("bar"), a = box("a"), grow = box("grow"), b = box("b");
    TEST_CHECK(bar.w == 2000 && bar.h == 100);                  // leftright: attached to both sides and stretched
    TEST_CHECK(a.x == 20 && a.y == 30 && a.w == 100);             // margin 20, centered (100 - 40) / 2
    TEST_CHECK(grow.x == 130 && grow.w == 2000 - 40 - 100 - 50 - 20 && grow.y == 40); // fill = remaining space
    TEST_CHECK(b.x == 2000 - 20 - 50 && b.y == 20);
    TEST_CHECK(box("corner").x == 1900 && box("corner").y == 450); // attached to the bottom right
    TEST_CHECK(box("middle").x == 950 && box("middle").y == 225);  // relative to the center
    u32 items = 0;
    for (const ui::NodeBox& n : sys.boxes(menu)) items += n.name == "item" ? 1u : 0u;
    TEST_CHECK(items == 3 && box("list").h == 3 * 30 + 2 * 5 && box("previewOnly").name.empty()); // the list has as many as the count, the example child is dropped
    TEST_CHECK(!box("extra").visible);
    vm.showExtra = true;
    vm.rowsCount = 4;
    sys.layout({0, 0, 2000, 500});
    TEST_CHECK(box("extra").visible && box("list").h == 4 * 30 + 3 * 5);

    TEST_CHECK(sys.press(menu, "b") && vm.sound); // toggles write their own field and fire an action
    TEST_CHECK(sys.actions().size() == 1 && sys.actions()[0].name == "changed" && sys.actions()[0].field == "sound");
    TEST_CHECK(sys.info(menu).errors.empty() && sys.info(menu).nodes == 1 + 4 + 2 + 1 + 4 + 1);

    // Grid: fixed/fr/auto/% tracks, cell spans, auto placement, in-cell alignment, content-fitted size
    TEST_CHECK(writeTextFile(dir + "/assets/ui/grid.ui",
                             "board grid 1000 500\n"
                             "  frame g\n    pos 0 0\n    size 1000 400\n    layout grid\n"
                             "    columns 100 1fr 2fr 10%\n    rows auto 1fr\n    gap 10 20\n    padding 10\n"
                             "    frame a\n      id p-a\n      size fill fill\n      cell 1 1\n    end\n"
                             "    frame tall\n      size 30 60\n      cell 1 2\n      justifySelf center\n    end\n"
                             "    frame wide\n      size fill 20\n      cell 2 2 1 2\n      alignSelf end\n    end\n"
                             "    frame autoPlaced\n      size 5 5\n    end\n"
                             "  end\n"
                             "  frame hug\n    pos 0 450\n    size hug hug\n    layout grid\n    columns auto auto\n    rows auto\n    gap 4\n"
                             "    frame x\n      size 40 10\n    end\n    frame y\n      size 60 30\n    end\n"
                             "  end\n"
                             "end\n"));
    const ui::ScreenId gridScreen = sys.open("ui/grid.ui");
    sys.layout({0, 0, 1000, 500});
    auto gbox = [&](const char* name) {
        for (const ui::NodeBox& b : sys.boxes(gridScreen))
            if (b.name == name) return b;
        return ui::NodeBox{};
    };
    // Inner 980×380, columns: 100, 10% = 98, remaining 980 - 100 - 98 - 60 = 722 → 1fr 240.67, 2fr 481.33. Rows: auto = 60 (tall), 1fr = 380 - 60 - 10 = 310
    const ui::NodeBox ga = gbox("a"), tall = gbox("tall"), wide = gbox("wide"), autoPlaced = gbox("autoPlaced");
    TEST_CHECK(sys.info(gridScreen).errors.empty());
    TEST_CHECK(ga.id == "p-a" && ga.x == 10 && ga.y == 10 && ga.w == 100 && ga.h == 60);
    TEST_CHECK(std::fabs(tall.x - (10 + 100 + 20 + (722.0f / 3.0f - 30) * 0.5f)) < 0.01f && tall.y == 10);
    TEST_CHECK(std::fabs(wide.w - (722.0f + 20)) < 0.01f && wide.y == 10 + 60 + 10 + 310 - 20); // two columns + gap between, bottom aligned
    TEST_CHECK(std::fabs(autoPlaced.x - (10 + 100 + 20 + 722.0f / 3.0f + 20)) < 0.01f && autoPlaced.y == 10); // first empty cell (row 1 column 3)
    TEST_CHECK(gbox("hug").w == 40 + 4 + 60 && gbox("hug").h == 30 && gbox("y").x == 44);

    const ui::ScreenId bad = sys.open("ui/bad.ui");
    const ui::ScreenInfo info = sys.info(bad);
    TEST_CHECK(info.errors.size() == 2 && info.errors[0].find("(3)") != std::string::npos && info.errors[1].find("(4)") != std::string::npos);
    // A list replaces the frame's widget, so scroll and list on one frame would never scroll: the parser says so
    TEST_CHECK(writeTextFile(dir + "/assets/ui/scrolllist.ui", "board s 100 100\n  frame x\n    scroll\n    list rows\n  end\nend\n"));
    const ui::ScreenInfo scrollList = sys.info(sys.open("ui/scrolllist.ui"));
    TEST_CHECK(scrollList.errors.size() == 1 && scrollList.errors[0].find("child frame") != std::string::npos);
    // The wheel scrolls a scroll area on a screen whose panel blocks the mouse (the settings screen shape)
    TEST_CHECK(writeTextFile(dir + "/assets/ui/scroller.ui",
                             "board sc 1000 500\n"
                             "  frame panel\n    pos 0 0\n    size 1000 500\n    fill #101010FF\n    layout column\n"
                             "    frame area\n      size 300 100\n      layout column\n      scroll\n"
                             "      frame items\n        size fill hug\n        layout column\n        gap 5\n        list rows\n"
                             "        frame item\n          size 300 30\n        end\n"
                             "      end\n"
                             "    end\n"
                             "  end\n"
                             "end\n"));
    sys.closeAll(); // screens above (the menu's filled bar) would take the mouse
    const ui::ScreenId scroller = sys.open("ui/scroller.ui");
    TestUiVm scrollVm;
    scrollVm.rowsCount = 4; // 4 × 30 + 3 × 5 = 135 in a 100 tall area
    sys.bind(scroller, scrollVm);
    sys.layout({0, 0, 1000, 500});
    auto itemsY = [&] {
        for (const ui::NodeBox& b : sys.boxes(scroller))
            if (b.name == "items") return b.y;
        return -1.0f;
    };
    TEST_CHECK(sys.info(scroller).errors.empty() && itemsY() == 0.0f);
    Input wheelInput;
    wheelInput.setMousePosition(50, 50);
    wheelInput.addWheel(-1.0f); // one notch down
    ui::FrameInput frame;
    frame.input = &wheelInput;
    sys.update(frame, {0, 0, 1000, 500}, 0.016);
    sys.layout({0, 0, 1000, 500});
    TEST_CHECK(itemsY() == -35.0f); // scrolled to the end (135 - 100), not past it
    setProjectRoot(oldRoot);
}
// The engine parser reads the Penpot plugin converter's expected files (tools/penpot-plugin/test/expected, made by node --test) without errors
TEST_CASE(ui, penpot_expected) {
    const std::string dir = std::string(MEALKIT_ENGINE_DIR) + "/tools/penpot-plugin/test/expected/";
    ui::Theme theme;
    std::string themeText;
    TEST_CHECK(readTextFile(dir + "theme.ui", themeText));
    // theme.ui is in theme format: color/number per line
    TEST_CHECK(themeText.rfind("# MealKit UI exported from Penpot", 0) == 0 && themeText.find("color accent #FFC83D") != std::string::npos);
    theme.colors.emplace_back("accent", 0xFF3DC8FFu);
    std::string text;
    TEST_CHECK(readTextFile(dir + "pause.ui", text));
    ui::Document doc;
    const bool ok = ui::parseDocument(text, "ui/pause.ui", doc, &theme);
    for (const std::string& e : doc.errors) std::printf("  %s\n", e.c_str());
    TEST_CHECK(ok && doc.exported && doc.navigation && doc.firstFocus == "resume" && doc.nodes.size() == 12);
    TEST_CHECK(doc.nodes[0].id == "id-pause");
}

// Editor "Make component": a frame moved into ui/components/<name>.ui + use lays out exactly as before (in a layout and free)
TEST_CASE(ui, ui_extract_component) {
    const std::string oldRoot = projectRoot();
    const std::string dir = executableDirectory() + "/test_ui_component";
    createDirectories(dir + "/assets/ui/components");
    const std::string screen = "board s 800 400\n"
                               "  frame bar\n    pos 0 0\n    size 800 100\n    layout row\n    gap 10\n    padding 10\n"
                               "    frame button\n      # the look\n      size hug 60\n      layout row\n      padding 8 16\n      fill #334455\n"
                               "      radius 6\n      button play\n      text label\n        content Play\n        fontSize 24\n      end\n    end\n"
                               "    frame grow\n      size fill 20\n    end\n"
                               "  end\n"
                               "  frame corner\n    pos 600 300\n    size 200 100\n    constraints right bottom\n    fill #FF0000\n"
                               "    frame dot\n      pos 10 10\n      size 20 20\n    end\n  end\n"
                               "end\n";
    TEST_CHECK(writeTextFile(dir + "/assets/ui/s.ui", screen));
    setProjectRoot(dir);
    auto layoutBoxes = [](const std::string& file) {
        ui::System sys;
        sys.init(nullptr, nullptr);
        const ui::ScreenId id = sys.open(file);
        sys.layout({0, 0, 1600, 800});
        return sys.boxes(id);
    };
    const std::vector<ui::NodeBox> before = layoutBoxes("ui/s.ui");

    ui::Source source;
    TEST_CHECK(ui::parseSource(screen, source));
    ui::Source a, b;
    TEST_CHECK(!ui::extractComponent(source, {}, "x", "ui/components/x.ui", 1, 1, a));      // the board
    TEST_CHECK(!ui::extractComponent(source, {0, 0, 0}, "x", "ui/components/x.ui", 1, 1, a)); // a text
    const ui::NodeBox* button = nullptr;
    for (const ui::NodeBox& box : before)
        if (box.name == "button") button = &box;
    TEST_CHECK(button && ui::extractComponent(source, {0, 0}, "big_button", "ui/components/big_button.ui", static_cast<i32>(button->w + 0.5f),
                                              static_cast<i32>(button->h + 0.5f), a));
    TEST_CHECK(ui::extractComponent(source, {1}, "panel", "ui/components/panel.ui", 200, 100, b));
    const ui::SourceNode* use = ui::findNode(source, {0, 0});
    std::string v;
    TEST_CHECK(use && use->kind == "use" && use->name == "button" && use->args == "ui/components/big_button.ui" && use->children.empty());
    TEST_CHECK(ui::getProperty(*use, "size", v) && v == "hug 60" && !ui::getProperty(*use, "fill", v));
    TEST_CHECK(ui::findNode(a, {})->name == "big_button" && ui::getProperty(*ui::findNode(a, {}), "button", v) && v == "play");
    TEST_CHECK(ui::findNode(a, {0}) && ui::findNode(a, {0})->name == "label");
    TEST_CHECK(ui::getProperty(*ui::findNode(source, {1}), "constraints", v) && !ui::getProperty(*ui::findNode(b, {}), "constraints", v));
    TEST_CHECK(writeTextFile(dir + "/assets/ui/components/big_button.ui", ui::writeSource(a)));
    TEST_CHECK(writeTextFile(dir + "/assets/ui/components/panel.ui", ui::writeSource(b)));
    TEST_CHECK(writeTextFile(dir + "/assets/ui/s2.ui", ui::writeSource(source)));
    ui::Source reread;
    TEST_CHECK(ui::parseSource(ui::writeSource(a), reread) && ui::writeSource(reread) == ui::writeSource(a));

    const std::vector<ui::NodeBox> after = layoutBoxes("ui/s2.ui");
    TEST_CHECK(after.size() == before.size());
    for (size_t i = 0; i < before.size() && i < after.size(); ++i) {
        const bool same = before[i].name == after[i].name && std::fabs(before[i].x - after[i].x) < 0.01f && std::fabs(before[i].y - after[i].y) < 0.01f &&
                          std::fabs(before[i].w - after[i].w) < 0.01f && std::fabs(before[i].h - after[i].h) < 0.01f;
        if (!same)
            std::printf("  %s %.1f %.1f %.1f %.1f -> %s %.1f %.1f %.1f %.1f\n", before[i].name.c_str(), before[i].x, before[i].y, before[i].w, before[i].h,
                        after[i].name.c_str(), after[i].x, after[i].y, after[i].w, after[i].h);
        TEST_CHECK(same);
    }
    setProjectRoot(oldRoot);
}

// Editor drafts: a smaller document set before the next layout must not leave boxes pointing past it (the add + delete crash)
TEST_CASE(ui, ui_draft_before_layout) {
    const std::string oldRoot = projectRoot();
    const std::string dir = executableDirectory() + "/test_ui_draft";
    createDirectories(dir + "/assets/ui");
    TEST_CHECK(writeTextFile(dir + "/assets/ui/d.ui", "board d 100 100\n  frame a\n    size 10 10\n  end\n  frame b\n    size 10 10\n  end\nend\n"));
    setProjectRoot(dir);
    ui::System sys;
    sys.init(nullptr, nullptr);
    const ui::ScreenId s = sys.open("ui/d.ui");
    sys.layout({0, 0, 100, 100});
    TEST_CHECK(sys.boxes(s).size() == 3);
    const std::string smaller = "board d 100 100\nend\n";
    sys.setDraft(s, &smaller);
    TEST_CHECK(sys.boxes(s).empty() && sys.info(s).focus.empty()); // nothing laid out for the new document yet
    sys.layout({0, 0, 100, 100});
    TEST_CHECK(sys.boxes(s).size() == 1);
    setProjectRoot(oldRoot);
}

// Editor preview inputs: focus moves like the ui.* actions and is reported by name
TEST_CASE(ui, ui_preview_focus) {
    const std::string oldRoot = projectRoot();
    const std::string dir = executableDirectory() + "/test_ui_focus";
    createDirectories(dir + "/assets/ui");
    TEST_CHECK(writeTextFile(dir + "/assets/ui/f.ui", "board f 100 100\n  navigation\n  firstFocus a\n"
                                                     "  frame a\n    pos 0 0\n    size 100 20\n    button x\n  end\n"
                                                     "  frame b\n    pos 0 50\n    size 100 20\n    button y\n  end\nend\n"));
    setProjectRoot(dir);
    {
        ui::System sys;
        sys.init(nullptr, nullptr);
        const ui::ScreenId s = sys.open("ui/f.ui");
        sys.layout({0, 0, 100, 100});
        TEST_CHECK(sys.info(s).focus.empty());
        sys.moveFocus(s, 1); // nothing focused: the first focus
        TEST_CHECK(sys.info(s).focus == "a");
        sys.moveFocus(s, 1);
        TEST_CHECK(sys.info(s).focus == "b");
        sys.moveFocus(s, 0);
        TEST_CHECK(sys.info(s).focus == "a");
    }
    setProjectRoot(oldRoot);
}

// Editable .ui source (editor [UI] panel): every .ui in the repo writes back unchanged, edits touch only their lines
TEST_CASE(ui, ui_source) {
    const std::string engine = std::string(MEALKIT_ENGINE_DIR);
    std::vector<std::string> dirs = {engine + "/templates/empty/assets/ui", engine + "/tools/penpot-plugin/test/expected"};
    for (const std::string& sample : listDirectories(engine + "/samples")) {
        dirs.push_back(engine + "/samples/" + sample + "/assets/ui");
        for (const std::string& module : listDirectories(engine + "/samples/" + sample + "/modules"))
            dirs.push_back(engine + "/samples/" + sample + "/modules/" + module + "/assets/ui");
    }
    u32 files = 0;
    for (const std::string& dir : dirs)
        for (const std::string& name : listFiles(dir, ".ui")) {
            if (name == "theme.ui") continue; // theme format, no board
            std::string text;
            TEST_CHECK(readTextFile(dir + "/" + name, text));
            std::string lf;
            for (char c : text)
                if (c != '\r') lf += c;
            ui::Source source;
            std::string error;
            const bool ok = ui::parseSource(lf, source, &error);
            if (!ok || ui::writeSource(source) != lf) std::printf("  %s/%s: %s\n", dir.c_str(), name.c_str(), ok ? "writes back differently" : error.c_str());
            TEST_CHECK(ok && ui::writeSource(source) == lf);
            ++files;
        }
    TEST_CHECK(files >= 20);

    // Sample screens with components: Document nodes map back to source nodes (a use subtree to the use node)
    for (const char* screen : {"ui/hud.ui", "ui/menu.ui", "ui/settings.ui"}) {
        std::string text;
        TEST_CHECK(readTextFile(findAssetFile(screen), text));
        ui::Source source;
        TEST_CHECK(ui::parseSource(text, source));
        ui::Document doc;
        ui::parseDocument(text, screen, doc);
        const std::vector<ui::SourcePath> map = ui::mapDocumentNodes(source, doc);
        TEST_CHECK(map.size() == doc.nodes.size() && !map.empty() && map[0].empty());
        for (size_t i = 0; i < map.size(); ++i) {
            const ui::SourceNode* n = ui::findNode(source, map[i]);
            TEST_CHECK(n && (n->kind == "use" || n->name == doc.nodes[i].name));
        }
    }

    const std::string text = "# header comment\n@format ui 1\nboard b 100 100\n  navigation\n  frame a\n    # about a\n    pos 1 2\n"
                             "    fill $panel\n    on hover fill #FFFFFF\n    text t\n      key x\n    end\n  end\n"
                             "  frame c\n  end\nend\n";
    ui::Source s;
    TEST_CHECK(ui::parseSource(text, s) && ui::writeSource(s) == text && !s.exported);
    ui::SourceNode* a = ui::findNode(s, {0});
    TEST_CHECK(a && a->name == "a" && ui::findNode(s, {0, 0})->name == "t" && !ui::findNode(s, {2}));
    std::string v;
    TEST_CHECK(ui::getProperty(*a, "fill", v) && v == "$panel" && ui::getProperty(*a, "on hover fill", v) && v == "#FFFFFF");
    ui::setProperty(*a, "pos", "10 20");          // replaced in place
    ui::setProperty(*a, "radius", "$radius");     // added after the last property, before the child
    ui::setProperty(*ui::findNode(s, {1}), "size", "50 hug"); // node without properties
    TEST_CHECK(ui::removeProperty(*a, "on hover fill") && !ui::removeProperty(*a, "on hover fill"));
    TEST_CHECK(ui::writeSource(s) == "# header comment\n@format ui 1\nboard b 100 100\n  navigation\n  frame a\n    # about a\n"
                                     "    pos 10 20\n    fill $panel\n    radius $radius\n    text t\n      key x\n    end\n  end\n"
                                     "  frame c\n    size 50 hug\n  end\nend\n");

    // Tree edits: add, move into another node and back between siblings, remove; the board can't move or go
    ui::SourceNode img;
    img.kind = "image";
    img.name = "icon";
    img.items.push_back({"src ui/icon.png", -1});
    TEST_CHECK((ui::addNode(s, {}, 1, img) == ui::SourcePath{1}) && ui::findNode(s, {2})->name == "c");
    ui::SourcePath moved;
    TEST_CHECK(ui::moveNode(s, {0, 0}, {2}, 0, &moved) && (moved == ui::SourcePath{2, 0})); // t from a into c
    TEST_CHECK(ui::findNode(s, {2, 0}) && ui::findNode(s, {2, 0})->name == "t" && ui::findNode(s, {0})->children.empty());
    TEST_CHECK(ui::moveNode(s, {2}, {}, 0, &moved) && (moved == ui::SourcePath{0}) && ui::findNode(s, {1})->name == "a");
    TEST_CHECK(!ui::moveNode(s, {0}, {0, 0}, 0) && !ui::moveNode(s, {}, {0}, 0) && !ui::removeNode(s, {}));
    TEST_CHECK(ui::removeNode(s, {2}) && !ui::findNode(s, {2}));
    TEST_CHECK(ui::writeSource(s) == "# header comment\n@format ui 1\nboard b 100 100\n  navigation\n  frame c\n    size 50 hug\n"
                                     "    text t\n      key x\n    end\n  end\n  frame a\n    # about a\n    pos 10 20\n    fill $panel\n"
                                     "    radius $radius\n  end\nend\n");
    ui::Document doc;
    ui::parseDocument(ui::writeSource(s), "ui/edited.ui", doc); // no theme here, so $radius is an error; the nodes are read anyway
    TEST_CHECK(doc.nodes.size() == 4 && doc.nodes[1].name == "c" && doc.nodes[2].name == "t" && doc.nodes[3].name == "a");

    // Clipboard text, unique names, wrapping
    const std::string clip = ui::writeNodeText(*ui::findNode(s, {0}));
    TEST_CHECK(clip == "frame c\n  size 50 hug\n  text t\n    key x\n  end\nend\n");
    ui::SourceNode pasted;
    TEST_CHECK(ui::parseNodeText(clip, pasted) && pasted.name == "c" && pasted.children.size() == 1);
    TEST_CHECK(!ui::parseNodeText("pos 1 2\n", pasted) && !ui::parseNodeText(clip + clip, pasted) && !ui::parseNodeText("frame a\n", pasted));
    TEST_CHECK(ui::uniqueName(s, "c") == "c2" && ui::uniqueName(s, "t") == "t2" && ui::uniqueName(s, "new") == "new");
    ui::SourceNode group;
    group.kind = "frame";
    group.name = "group";
    ui::SourcePath wrapped;
    TEST_CHECK(ui::wrapNode(s, {1}, group, &wrapped) && (wrapped == ui::SourcePath{1}));
    TEST_CHECK(ui::findNode(s, {1})->name == "group" && ui::findNode(s, {1, 0})->name == "a" && !ui::wrapNode(s, {}, group));

    ui::Source bad;
    std::string error;
    TEST_CHECK(!ui::parseSource("board b 1 1\n  frame a\nend\n", bad, &error) && error.find("missing end") != std::string::npos);
    TEST_CHECK(!ui::parseSource("frame a\nend\n", bad, &error) && !ui::parseSource("board b 1 1\nend\nend\n", bad, &error));
}

TEST_CASE(settings, settings_service) {
    const std::string oldRoot = projectRoot();
    const std::string dir = executableDirectory() + "/test_settings";
    createDirectories(dir + "/assets/ui");
    TEST_CHECK(writeTextFile(dir + "/assets/ui/settings.ui",
                             "board settings 1000 1000\n"
                             "  frame rows\n    size 1000 hug\n    layout column\n    list rows\n"
                             "    frame row\n      size 1000 40\n      frame toggle\n        size 40 40\n        visible isToggle\n"
                             "        toggle on settings.changed\n      end\n    end\n  end\n"
                             "  frame close\n    pos 0 900\n    size 100 50\n    button settings.close\n  end\n"
                             "end\n"));
    const std::string file = dir + "/user/settings.ini";
    removeFile(file);
    setProjectRoot(dir);
    {
        ui::System sys;
        sys.init(nullptr, nullptr);
        settings::Service s;
        settings::Service::Targets t;
        t.ui = &sys;
        s.init(t, file);
        TEST_CHECK(s.values().display.vsync && !s.isOpen());
        s.open();
        TEST_CHECK(s.isOpen());
        sys.layout({0, 0, 1000, 1000});
        // Display tab: window mode, resolution, vsync (first toggle) ... → pressing the toggle changes and saves the setting
        TEST_CHECK(sys.press(1, "toggle"));
        s.update(nullptr, 0.0);
        TEST_CHECK(!s.values().display.vsync && fileExists(file));
        TEST_CHECK(sys.press(1, "close"));
        s.update(nullptr, 0.0);
        TEST_CHECK(!s.isOpen());
    }
    {
        settings::Service again; // rereading gives the changed value
        again.init({}, file);
        TEST_CHECK(!again.values().display.vsync && again.values().audio.master == 1.0f);
    }
    setProjectRoot(oldRoot);
}

// The Blender add-on's supported range schema matches engine loading. If different, rewrite the file with mealkit_tests --write-asset-schema
TEST_CASE(asset, asset_schema) {
    const std::string path = std::string(MEALKIT_ENGINE_DIR) + "/tools/blender-addon/mealkit/schema.json";
    const std::string expected = asset::modelSchemaJson();
    std::string text;
    TEST_CHECK(readTextFile(path, text));
    std::erase(text, '\r'); // git autocrlf
    TEST_CHECK(text == expected);
}

// Style preset material features: mealkit_ keys in glTF extras (Blender custom properties) → material bits and values, bad keys/values become problems
TEST_CASE(asset, material_extras) {
    render::MaterialData m;
    std::vector<std::string> problems;
    asset::applyMaterialExtras(R"({"mealkit_outline": 0.05, "mealkit_outlineColor": [0.1, 0.2, 0.3], "other": {"a": [1, "x"]},
                                   "mealkit_rimColor": [1, 0, 0], "mealkit_wind": 1e-1})",
                               m, problems);
    TEST_CHECK(problems.empty());
    TEST_CHECK(m.features == (render::kMaterialOutline | render::kMaterialWind)); // a rim with only color isn't turned on
    TEST_CHECK(m.outlineWidth == 0.05f && m.outlineColor[2] == 0.3f && m.rimColor[1] == 0.0f && m.windStrength == 0.1f);

    render::MaterialData d;
    asset::applyMaterialExtras(R"({"mealkit_dissolve": [1, 2], "mealkit_rim": "big", "mealkit_glow": 1, "mealkit_rimColor": 2})", d,
                               problems);
    TEST_CHECK(problems.size() == 4 && d.features == 0);
    asset::applyMaterialExtras(nullptr, d, problems); // no extras
    asset::applyMaterialExtras("not json", d, problems);
    TEST_CHECK(problems.size() == 4 && d.features == 0);

    // Instance values in the upper 16 bits of flags (still 48 bytes)
    const u32 flags = render::instanceDissolve(render::instanceFlash(render::kInstanceHighlight, 1.0f), 0.5f);
    TEST_CHECK((flags & 0xFFFFu) == render::kInstanceHighlight && ((flags >> render::kInstanceFlashShift) & 0xFFu) == 255 &&
               (flags >> render::kInstanceDissolveShift) == 128);
    TEST_CHECK(render::instanceFlash(flags, 0.0f) >> render::kInstanceFlashShift == 128u << 8);
}

// Sky HDRI baking: RGB9E5 round trip, cube face directions, a uniform sky of brightness 1 gives 1 for ambient/reflection/sky
// Color grading LUT (.cube): tables of other sizes are trilinearly resampled to 32 cells, DOMAIN is honored, 1D/short tables are rejected
TEST_CASE(asset, terrain_heightmap) {
    // PNG round trip (16-bit, rows flipped so the image top is +z)
    asset::Heightmap map = asset::flatHeightmap(33, 1000);
    for (u32 z = 0; z < 33; ++z)
        for (u32 x = 0; x < 33; ++x) map.samples[z * 33 + x] = static_cast<u16>(x * 1900 + z * 7);
    const std::vector<u8> png = asset::encodeHeightmap(map);
    asset::Heightmap back;
    std::string error;
    TEST_CHECK(asset::decodeHeightmap(png.data(), png.size(), back, error));
    TEST_CHECK(back.size == 33 && back.samples == map.samples && back.hash() == map.hash());

    // Surface height matches the samples, and the physics height field agrees with it
    const Vec3 center{10.0f, 5.0f, -4.0f}, size{64.0f, 20.0f, 64.0f};
    f32 y = 0.0f;
    TEST_CHECK(asset::terrainHeightAt(map, center, size, center.x - 32.0f, center.z - 32.0f, &y));
    TEST_CHECK(std::fabs(y - (center.y - 10.0f + 20.0f * map.at(0, 0) / 65535.0f)) < 1e-3f);
    TEST_CHECK(!asset::terrainHeightAt(map, center, size, center.x + 40.0f, center.z, &y));
    physics::PhysicsWorld w;
    w.init(64, 1);
    std::vector<f32> heights;
    asset::terrainHeights(map, size.y, heights);
    physics::BodyDesc d;
    d.position = center;
    TEST_CHECK(w.createHeightField(heights.data(), map.size, size * -0.5f, size.x / 32.0f, size.z / 32.0f, d) != physics::kInvalidBody);
    const f32 px = center.x + 7.3f, pz = center.z - 3.1f;
    TEST_CHECK(asset::terrainHeightAt(map, center, size, px, pz, &y));
    physics::RayHit hit;
    TEST_CHECK(w.castRay({px, 100.0f, pz}, {0.0f, -200.0f, 0.0f}, hit));
    TEST_CHECK(std::fabs((100.0f - 200.0f * hit.fraction) - y) < 0.05f); // Jolt quantizes heights slightly
    const f32 t = asset::raycastTerrain(map, center, size, {px, 100.0f, pz}, {0.0f, -1.0f, 0.0f}, 1000.0f);
    TEST_CHECK(std::fabs((100.0f - t) - y) < 0.01f);
    w.shutdown();

    // Chunks: 32 cells -> one chunk, LODs halve the grid; brushes only touch samples inside the radius
    TEST_CHECK(asset::terrainChunks(map) == 1);
    render::MeshData lod0, lod1;
    asset::buildTerrainChunk(map, size, 0, 0, 0, lod0);
    asset::buildTerrainChunk(map, size, 0, 0, 1, lod1);
    TEST_CHECK(lod0.vertices.size() == 33 * 33 + 4 * 33 && lod1.vertices.size() == 17 * 17 + 4 * 17);
    asset::Heightmap flat = asset::flatHeightmap(65, 20000);
    const asset::SampleRect rect = asset::applyBrush(flat, {}, {64.0f, 10.0f, 64.0f}, asset::TerrainBrush::Raise, {0, 0, 0}, 4.0f, 0.1f, 0);
    TEST_CHECK(!rect.empty && flat.at(32, 32) > 20000 && flat.at(0, 0) == 20000 && flat.at(40, 32) == 20000);
}

TEST_CASE(asset, scatter_layer) {
    // Painting keeps spacing, stays inside the circle and only where there is floor; erase and grouping; file round trip
    asset::Scatter s;
    asset::ScatterPaint p;
    p.center = {10.0f, 0.0f, 10.0f};
    p.radius = 8.0f;
    p.density = 1.0f;
    u64 seed = 3;
    auto random01 = [&] {
        seed = seed * 6364136223846793005ull + 1442695040888963407ull;
        return static_cast<f32>(seed >> 40) / 16777216.0f;
    };
    auto floorAt = [](f32 x, f32, f32* y) { // floor only on the x > 10 half, at height 2
        *y = 2.0f;
        return x > 10.0f;
    };
    const u32 added = asset::paintScatter(s, p, 500, floorAt, random01);
    TEST_CHECK(added > 20 && added == s.items.size());
    for (size_t i = 0; i < s.items.size(); ++i) {
        const asset::ScatterInstance& a = s.items[i];
        TEST_CHECK(a.position.x > 10.0f && a.position.y == 2.0f && length(a.position - Vec3{10.0f, 2.0f, 10.0f}) <= 8.0f);
        TEST_CHECK(a.scale >= 0.8f && a.scale <= 1.2f);
        for (size_t j = i + 1; j < s.items.size(); ++j) TEST_CHECK(length(a.position - s.items[j].position) >= 0.75f - 1e-4f);
    }
    asset::groupScatter(s);
    u32 grouped = 0;
    for (const asset::ScatterCell& c : s.cells) grouped += c.count;
    TEST_CHECK(s.grouped && grouped == s.items.size() && s.cells.size() == 1); // all inside one 32 m cell
    const std::string file = executableDirectory() + "/test_layer.scatter";
    std::string error;
    TEST_CHECK(asset::saveScatter(file, s, error));
    asset::Scatter back;
    TEST_CHECK(asset::loadScatter(file, back, error) && back.items.size() == s.items.size());
    TEST_CHECK(std::fabs(back.items[0].position.x - s.items[0].position.x) < 1e-3f);
    removeFile(file);
    const u32 removed = asset::eraseScatter(s, {14.0f, 0.0f, 10.0f}, 3.0f);
    TEST_CHECK(removed > 0 && s.items.size() == added - removed && !s.grouped);
}

TEST_CASE(asset, cube_lut) {
    // 2-cell table: swaps red and blue (interpolation is linear, so it swaps exactly anywhere in the 32 cells)
    const std::string swap = "TITLE \"swap\"\n# comment\nLUT_3D_SIZE 2\n"
                             "0 0 0\n0 0 1\n0 1 0\n0 1 1\n1 0 0\n1 0 1\n1 1 0\n1 1 1\n";
    std::vector<f32> lut;
    std::string error;
    TEST_CHECK(render::parseCubeLut(swap, lut, error));
    constexpr u32 n = render::kLutSize;
    TEST_CHECK(lut.size() == static_cast<size_t>(n) * n * n * 3);
    const auto at = [&](u32 r, u32 g, u32 b, u32 c) { return lut[((static_cast<size_t>(b) * n + g) * n + r) * 3 + c]; };
    TEST_CHECK(std::abs(at(31, 0, 0, 2) - 1.0f) < 1e-5f && at(31, 0, 0, 0) < 1e-5f); // red → blue
    TEST_CHECK(std::abs(at(10, 20, 5, 0) - 5.0f / 31.0f) < 1e-5f && std::abs(at(10, 20, 5, 1) - 20.0f / 31.0f) < 1e-5f);

    // The neutral LUT file reads back as identity
    const std::string path = std::string(MEALKIT_ENGINE_DIR) + "/build/check/neutral_test.cube";
    createDirectories(parentDirectory(path));
    TEST_CHECK(render::saveNeutralCubeLut(path));
    TEST_CHECK(render::loadCubeLut(path, lut, error));
    TEST_CHECK(std::abs(at(7, 13, 29, 0) - 7.0f / 31.0f) < 1e-5f && std::abs(at(7, 13, 29, 2) - 29.0f / 31.0f) < 1e-5f);
    removeFile(path);

    TEST_CHECK(!render::parseCubeLut("LUT_1D_SIZE 4\n0 0 0\n", lut, error));
    TEST_CHECK(!render::parseCubeLut("LUT_3D_SIZE 2\n0 0 0\n", lut, error) && error.find("expected 8") != std::string::npos);
}

TEST_CASE(asset, environment_bake) {
    for (const f32 v : {0.0f, 0.001f, 0.5f, 1.0f, 3.7f, 250.0f}) {
        f32 back[3];
        render::unpackRgb9e5(render::packRgb9e5(v, v * 0.5f, 0.0f), back);
        TEST_CHECK(std::abs(back[0] - v) <= v * 0.01f + 1e-6f && std::abs(back[1] - v * 0.5f) <= v * 0.01f + 1e-6f && back[2] == 0.0f);
    }
    f32 neg[3];
    render::unpackRgb9e5(render::packRgb9e5(-1.0f, 1e9f, 0.25f), neg); // negatives become 0, too large becomes max (the shared exponent makes small channels 0)
    TEST_CHECK(neg[0] == 0.0f && neg[1] > 60000.0f && neg[2] < 1.0f);
    const Vec3 px = render::cubeDirection(0, 0, 0), py = render::cubeDirection(2, 0, 0), nz = render::cubeDirection(5, 0, 0);
    TEST_CHECK(px.x > 0.99f && py.y > 0.99f && nz.z < -0.99f);
    const Vec3 corner = render::cubeDirection(4, 1, -1); // top right of the +Z face = +x, +y
    TEST_CHECK(corner.x > 0.5f && corner.y > 0.5f && corner.z > 0.5f);

    std::vector<f32> flat(64 * 32 * 3, 1.0f);
    render::EnvironmentMap env;
    render::bakeEnvironment(flat.data(), 64, 32, env);
    TEST_CHECK(env.skySize == 16 && env.sky.size() == 16u * 16 * 6 && env.reflectMips == 6);
    for (const Vec3 n : {Vec3{0, 1, 0}, Vec3{0, -1, 0}, Vec3{1, 0, 0}, normalize(Vec3{1, 1, -1})}) {
        const Vec3 a = render::evalSh(env.sh, n);
        TEST_CHECK(std::abs(a.x - 1.0f) < 0.03f && std::abs(a.y - 1.0f) < 0.03f && std::abs(a.z - 1.0f) < 0.03f);
    }
    f32 texel[3];
    render::unpackRgb9e5(env.sky[123], texel);
    TEST_CHECK(std::abs(texel[0] - 1.0f) < 0.02f);
    render::unpackRgb9e5(env.reflect.back(), texel); // roughest mip
    TEST_CHECK(std::abs(texel[1] - 1.0f) < 0.02f);
}

// Reflection probes: the cube face cameras the editor draws have the same layout as cubeDirection, and baking/file round trips preserve values
TEST_CASE(asset, reflection_probe) {
    static const Vec3 kForward[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}}; // same as editor/Editor.cpp
    static const Vec3 kUp[6] = {{0, 1, 0}, {0, 1, 0}, {0, 0, -1}, {0, 0, 1}, {0, 1, 0}, {0, 1, 0}};
    const Mat4 proj = perspectiveReversedZ(3.14159265f * 0.5f, 1.0f, 0.05f, 100.0f);
    for (u32 face = 0; face < 6; ++face) {
        const Mat4 viewProj = lookAtLH({}, kForward[face], kUp[face]) * proj;
        for (const f32 s : {-0.5f, 0.25f})
            for (const f32 t : {-0.75f, 0.5f}) {
                const Vec3 d = render::cubeDirection(face, s, t);
                const f32 x = d.x * viewProj.m[0][0] + d.y * viewProj.m[1][0] + d.z * viewProj.m[2][0] + viewProj.m[3][0];
                const f32 y = d.x * viewProj.m[0][1] + d.y * viewProj.m[1][1] + d.z * viewProj.m[2][1] + viewProj.m[3][1];
                const f32 w = d.x * viewProj.m[0][3] + d.y * viewProj.m[1][3] + d.z * viewProj.m[2][3] + viewProj.m[3][3];
                TEST_CHECK(std::abs(x / w - s) < 1e-4f && std::abs(y / w + t) < 1e-4f); // image rows go top to bottom (t = -NDC y)
            }
    }
    // A different color per face: the mirror mip is that face's color, the roughest mip the blended color
    const u32 size = 32;
    std::vector<f32> faces(size * size * 18);
    for (u32 face = 0; face < 6; ++face)
        for (u32 i = 0; i < size * size; ++i) faces[(face * size * size + i) * 3 + face % 3] = 1.0f;
    render::ProbeMap probe;
    render::bakeProbe(faces.data(), size, probe);
    TEST_CHECK(probe.size == 32 && probe.mips == 4); // 32 → 4
    f32 texel[3];
    render::unpackRgb9e5(probe.reflect[size * size / 2 + size / 2], texel); // center of the +X face = red
    TEST_CHECK(texel[0] > 0.95f && texel[1] < 0.05f && texel[2] < 0.05f);
    const std::string path = executableDirectory() + "/test_probe.mkprobe";
    TEST_CHECK(render::saveProbe(path, probe));
    render::ProbeMap back;
    std::string error;
    TEST_CHECK(render::loadProbe(path, back, error) && back.size == probe.size && back.mips == probe.mips && back.reflect == probe.reflect);
    removeFile(path);
    TEST_CHECK(!render::loadProbe(path, back, error) && !error.empty());
}

// Point lights: a built-in kind but with fields; scene object → render::PointLight (world position, linear color × intensity, disabled lights excluded)
TEST_CASE(render, scene_point_lights) {
    const reflect::TypeInfo* type = scene::findType(scene::kTypePointLight, nullptr);
    TEST_CHECK(type && type->field("range") && (type->flags & reflect::kObjectCosmetic));
    scene::Scene s;
    scene::Entity& parent = s.add(scene::kTypeSolid, "Parent");
    parent.position = {10, 0, 0};
    const u32 parentId = parent.id; // if add grows the vector, earlier references become invalid
    scene::Entity& lamp = s.add(scene::kTypePointLight, "Lamp");
    lamp.parent = parentId;
    lamp.position = {0, 2, 0};
    lamp.fields = {{"color", "#FFFFFF"}, {"intensity", "2"}, {"range", "5"}};
    scene::Entity& off = s.add(scene::kTypePointLight, "Off");
    off.fields = {{"intensity", "0"}};
    std::vector<render::PointLight> lights;
    render::addSceneLights(s, lights);
    TEST_CHECK(lights.size() == 1);
    TEST_CHECK(lights[0].position.x == 10.0f && lights[0].position.y == 2.0f && lights[0].range == 5.0f);
    TEST_CHECK(std::abs(lights[0].color.x - 2.0f) < 1e-5f);
    TEST_CHECK(std::abs(render::srgbToLinear(Color::rgb(188, 188, 188)).x - 0.5f) < 0.01f);
}

// Skeletal animation: skin/clips of the sample figure (rig_figure.glb), rest pose palette = identity, tick sampling = seconds sampling of the same frame
TEST_CASE(asset, skeletal_animation) {
    asset::ModelCache models; // without a renderer (CPU data only)
    const asset::Model* rig = models.get("models/characters/rig_figure.glb");
    TEST_CHECK(rig && rig->skinned() && rig->bones == 8 && rig->clips.size() == 3);
    if (!rig || !rig->skinned()) return;
    for (size_t j = 0; j < rig->skeleton.joints.size(); ++j) // parents first
        TEST_CHECK(rig->skeleton.joints[j].parent < static_cast<i32>(j));
    TEST_CHECK(rig->skeleton.joints[0].name == "root" && asset::findClip(rig->clips, "run") && !asset::findClip(rig->clips, "fly"));
    std::vector<asset::JointPose> pose;
    std::vector<asset::BoneMatrix> palette;
    asset::restPose(rig->skeleton, pose);
    asset::skinPalette(rig->skeleton, pose, palette);
    for (const asset::BoneMatrix& b : palette)
        for (int r = 0; r < 3; ++r)
            for (int c = 0; c < 4; ++c) TEST_CHECK(std::abs(b.rows[r][c] - (r == c ? 1.0f : 0.0f)) < 1e-5f);
    const asset::AnimationClip& run = *asset::findClip(rig->clips, "run");
    TEST_CHECK(run.frames == 19 && std::abs(run.duration - 0.6f) < 1e-5f); // 0.6 s × 30 Hz + 1
    std::vector<asset::JointPose> byTick, bySeconds;
    asset::samplePoseTick(rig->skeleton, run, 9, 30, true, byTick); // tick 9 @30 Hz = frame 9
    asset::samplePose(rig->skeleton, run, 9.0f / 30.0f, true, bySeconds);
    for (size_t j = 0; j < byTick.size(); ++j)
        TEST_CHECK(std::abs(byTick[j].rotation.x - bySeconds[j].rotation.x) < 1e-5f && std::abs(byTick[j].translation.y - bySeconds[j].translation.y) < 1e-5f);
    // Leg-swinging frame: the left leg joint moved, and the toe transformed by the palette differs from the rest pose (including z flip)
    asset::samplePose(rig->skeleton, run, 0.15f, true, pose);
    asset::skinPalette(rig->skeleton, pose, palette);
    const i32 leg = [&] { for (size_t j = 0; j < rig->skeleton.joints.size(); ++j) if (rig->skeleton.joints[j].name == "leg.L") return static_cast<i32>(j); return -1; }();
    TEST_CHECK(leg > 0);
    const asset::BoneMatrix& m = palette[static_cast<size_t>(leg)];
    const Vec3 foot{0.15f, 0.0f, 0.0f}; // MealKit coordinates (z-flipped vertex)
    const Vec3 moved{m.rows[0][0] * foot.x + m.rows[0][1] * foot.y + m.rows[0][2] * foot.z + m.rows[0][3],
                     m.rows[1][0] * foot.x + m.rows[1][1] * foot.y + m.rows[1][2] * foot.z + m.rows[1][3],
                     m.rows[2][0] * foot.x + m.rows[2][1] * foot.y + m.rows[2][2] * foot.z + m.rows[2][3]};
    TEST_CHECK(std::abs(moved.z) > 0.2f && moved.y > 0.0f); // 35 degree swing: the foot moves forward or back, and up
    std::vector<Mat4> world;
    asset::jointMatrices(rig->skeleton, pose, world);
    const Mat4& hip = world[static_cast<size_t>(leg)]; // leg joint = beside the hip (z flip of 0 is the same)
    TEST_CHECK(std::abs(length(moved - Vec3{hip.m[3][0], hip.m[3][1], -hip.m[3][2]}) - 0.7f) < 1e-3f); // leg length unchanged
}

// Animation state table: parsing, transitions (conditions, triggers, end), 1D blend, crossfade, events once per cycle crossing
TEST_CASE(asset, camera_clip) {
    const std::string path = findAssetFile("models/cutscenes/intro.glb");
    TEST_CHECK(!path.empty());
    std::vector<asset::CameraClip> clips;
    std::string error;
    TEST_CHECK(asset::loadCameraClips(path, clips, error));
    if (clips.empty()) return;
    TEST_CHECK(clips[0].name == "intro");
    TEST_CHECK(std::fabs(clips[0].duration - 3.0f) < 1e-4f && clips[0].keys.size() == 91);
    // First key: generator eye (sin(-60) * 34, 20, cos(-60) * 34) in glTF → z flipped, looking at the arena center
    const asset::CameraKey k = asset::sampleCamera(clips[0], 0.0f);
    TEST_CHECK(std::fabs(k.position.x + 29.4449f) < 1e-3f && std::fabs(k.position.y - 20.0f) < 1e-3f &&
               std::fabs(k.position.z + 17.0f) < 1e-3f);
    TEST_CHECK(dot(rotate(k.rotation, Vec3{0, 0, 1}), normalize(Vec3{0, 1, 0} - k.position)) > 0.9999f);
    TEST_CHECK(std::fabs(k.fovDegrees - 50.0f) < 1e-3f);
    TEST_CHECK(!asset::loadCameraClips(findAssetFile("models/tree.glb"), clips, error) && error == "no camera node");

    sim::Table<asset::CutsceneRow> table;
    table.ids = {"intro", "broken"};
    table.rows.resize(2);
    table.rows[0].file = "models/cutscenes/intro.glb";
    table.rows[0].events = "2 B 0.5 A";
    table.rows[1].file = "models/missing.glb";
    table.rows[1].events = "x A 1";
    asset::Cutscenes cutscenes;
    std::string warnings;
    cutscenes.build(table, &warnings);
    TEST_CHECK(warnings.find("broken: events need") != std::string::npos && warnings.find("bad event time 'x'") != std::string::npos &&
               warnings.find("not found") != std::string::npos);
    const asset::Cutscenes::Cutscene* intro = cutscenes.find("intro");
    TEST_CHECK(intro && intro->clip && intro->events.size() == 2 && intro->events[0].second == "A");

    // Brain playback: events in order, input blocked, back to the game camera at the end
    camera::Brain brain;
    brain.load(scene::Scene{});
    TEST_CHECK(brain.playCutscene(cutscenes, "intro") && !brain.playCutscene(cutscenes, "broken") && brain.blocksInput());
    std::vector<std::string> events;
    camera::BrainInput in;
    in.dt = 0.1f;
    for (int i = 0; i < 29; ++i) brain.update(in, &events);
    TEST_CHECK(events.size() == 2 && events[0] == "A" && events[1] == "B" && brain.cutscenePlaying());
    for (int i = 0; i < 3; ++i) brain.update(in, &events);
    TEST_CHECK(!brain.cutscenePlaying() && !brain.blocksInput() && brain.currentName().empty());
}

TEST_CASE(camera, brain) {
    TEST_CHECK(dot(rotate(camera::lookRotation({1, 2, 3}), Vec3{0, 0, 1}), normalize(Vec3{1, 2, 3})) > 0.9999f);
    TEST_CHECK(dot(rotate(camera::lookRotation({0, -1, 0}), Vec3{0, 0, 1}), Vec3{0, -1, 0}) > 0.9999f);
    scene::Scene s;
    scene::Entity& a = s.add(scene::kTypeVirtualCamera, "follow");
    a.fields = scene::defaultFields(*scene::findType(scene::kTypeVirtualCamera, nullptr));
    scene::Entity& b = s.add(scene::kTypeVirtualCamera, "overview");
    b.position = {0, 10, -10};
    b.fields = {{"body", "Fixed"}, {"priority", "20"}, {"active", "false"}, {"blendSeconds", "1"}, {"ease", "Linear"}};
    camera::Brain brain;
    brain.load(s);
    camera::BrainInput in;
    in.dt = 0.1f;
    in.hasTarget = true;
    in.target = {2, 0, 3};
    brain.update(in);
    const Vec3 focus = in.target + Vec3{0, 0.8f, 0};
    TEST_CHECK(brain.currentName() == "follow" && std::fabs(length(brain.pose().position - focus) - 18.0f) < 1e-3f);
    TEST_CHECK(dot(brain.pose().forward(), normalize(focus - brain.pose().position)) > 0.9999f);
    // Spring arm: a wall halfway pulls the camera in (margin 0.3 m)
    in.ray = [](Vec3, Vec3, f32& fraction) { return fraction = 0.5f, true; };
    brain.update(in);
    TEST_CHECK(std::fabs(length(brain.pose().position - focus) - (18.3f * 0.5f - 0.3f)) < 1e-3f);
    in.ray = nullptr;
    // Higher priority takes over with a 1 s linear blend; game code only switches active
    TEST_CHECK(brain.setActive("overview", true) && !brain.setActive("nothing", true));
    brain.update(in);
    TEST_CHECK(brain.currentName() == "overview" && length(brain.pose().position - b.position) > 1.0f);
    for (int i = 0; i < 10; ++i) brain.update(in);
    TEST_CHECK(length(brain.pose().position - b.position) < 1e-4f);
    // Lower its priority: back to the follow camera; none active: the fallback (first person here)
    brain.setPriority("overview", 0);
    brain.update(in);
    TEST_CHECK(brain.currentName() == "follow");
    brain.setActive("follow", false);
    brain.setActive("overview", false);
    scene::VirtualCameraFields first;
    first.body = scene::CameraBody::FirstPerson;
    first.blendSeconds = 0.0f;
    brain.setFallback(first);
    brain.update(in);
    TEST_CHECK(brain.currentName().empty() && length(brain.pose().position - focus) < 1e-4f);
    // Shake is added on top
    in.shake = {0, 0.5f, 0};
    brain.update(in);
    TEST_CHECK(std::fabs(brain.pose().position.y - focus.y - 0.5f) < 1e-4f);
}

TEST_CASE(asset, animation_states) {
    asset::ModelCache models;
    const asset::Model* rig = models.get("models/characters/rig_figure.glb");
    TEST_CHECK(rig && rig->skinned());
    if (!rig || !rig->skinned()) return;
    sim::Table<asset::AnimState> table;
    table.rows.reserve(4); // row() returns pointers into rows
    const auto row = [&](const char* id, const char* clip, const char* transitions) {
        table.ids.push_back(id);
        table.rows.emplace_back();
        table.rows.back().clip = clip;
        table.rows.back().transitions = transitions;
        return &table.rows.back();
    };
    row("pawn.idle", "idle", "move if speed > 0.6 and grounded; wave on push; nowhere if x");
    asset::AnimState* move = row("pawn.move", "run 3 idle 0", "idle if speed < 0.4; bad");
    move->blendParam = "speed";
    move->blendIn = 0.2f;
    move->events = "0.25 Footstep 0.75 Footstep";
    asset::AnimState* wave = row("pawn.wave", "wave", "idle if end");
    wave->loop = false;
    wave->blendIn = 0.0f;
    row("crate", "idle", "");
    asset::AnimStates states;
    std::string warnings;
    states.build(table, &warnings);
    TEST_CHECK(states.setCount() == 1 && states.findSet("pawn") == 0 && states.findSet("crate") < 0);
    TEST_CHECK(warnings.find("crate") != std::string::npos && warnings.find("unknown state") != std::string::npos &&
               warnings.find("'bad'") != std::string::npos);    const asset::AnimStates::Set& set = states.set(0);
    TEST_CHECK(set.states.size() == 3 && set.states[1].points.size() == 2 && set.states[1].points[0].clip == "idle"); // sorted by value
    TEST_CHECK(set.states[0].transitions.size() == 2 && set.states[0].transitions[0].conds.size() == 2);

    asset::AnimInstance anim;
    asset::AnimParams params;
    std::vector<std::string> events;
    const f32 dt = 1.0f / 60.0f;
    asset::updateAnim(states, 0, rig->clips, anim, params, dt, &events);
    TEST_CHECK(anim.set == 0 && anim.state == 0 && anim.fade == 1.0f);
    params.set("speed", 2.0f); // not grounded yet: both conditions must hold
    asset::updateAnim(states, 0, rig->clips, anim, params, dt, &events);
    TEST_CHECK(anim.state == 0);
    params.set("grounded", true);
    asset::updateAnim(states, 0, rig->clips, anim, params, dt, &events);
    TEST_CHECK(anim.state == 1 && anim.from == 0 && anim.fade > 0.0f && anim.fade < 1.0f && anim.blend == 2.0f);
    std::vector<asset::JointPose> pose;
    asset::animPose(states, rig->skeleton, rig->clips, anim, pose);
    TEST_CHECK(pose.size() == rig->skeleton.joints.size());
    // Two full cycles (cycle = lerp of idle/run lengths at speed 2): four footsteps, fade finished
    const f32 cycle = [&] {
        const f32 i = asset::findClip(rig->clips, "idle")->duration, r = asset::findClip(rig->clips, "run")->duration;
        return i + (r - i) * (2.0f / 3.0f);
    }();
    events.clear();
    for (f32 t = 0.0f; t < cycle * 2.0f - dt; t += dt) asset::updateAnim(states, 0, rig->clips, anim, params, dt, &events);
    TEST_CHECK(events.size() == 4 && events[0] == "Footstep" && anim.fade == 1.0f && anim.state == 1);
    // Trigger from move isn't listed → stays; slowing down returns to idle, then the trigger plays wave until its end
    params.trigger("push");
    asset::updateAnim(states, 0, rig->clips, anim, params, dt, &events);
    TEST_CHECK(anim.state == 1 && params.triggers.empty());
    params.set("speed", 0.0f);
    asset::updateAnim(states, 0, rig->clips, anim, params, dt, &events);
    TEST_CHECK(anim.state == 0);
    params.trigger("push");
    asset::updateAnim(states, 0, rig->clips, anim, params, dt, &events);
    TEST_CHECK(anim.state == 2 && anim.fade == 1.0f);
    const f32 waveSeconds = asset::findClip(rig->clips, "wave")->duration;
    for (f32 t = 0.0f; t < waveSeconds + 0.1f; t += dt) asset::updateAnim(states, 0, rig->clips, anim, params, dt, &events);
    TEST_CHECK(anim.state == 0);
    // Unknown set resets
    asset::updateAnim(states, 5, rig->clips, anim, params, dt, &events);
    TEST_CHECK(anim.set == -1);
}

// Texture cache mipmaps: size halves, sRGB averages in linear, normals renormalize to length 1. half conversion
TEST_CASE(asset, texture_mips) {
    std::vector<u8> rgba(4 * 2 * 4); // 4x2: left half white, right half black
    for (u32 y = 0; y < 2; ++y)
        for (u32 x = 0; x < 4; ++x)
            for (u32 c = 0; c < 4; ++c) rgba[(y * 4 + x) * 4 + c] = (x < 2 || c == 3) ? 255 : 0;
    std::vector<std::vector<u8>> mips;
    render::buildMips(rgba.data(), 4, 2, true, false, mips);
    TEST_CHECK(mips.size() == 3 && mips[1].size() == 2 * 1 * 4 && mips[2].size() == 4);
    TEST_CHECK(mips[2][0] == 188 && mips[2][3] == 255); // linear average of white/black 0.5 = sRGB 188
    render::buildMips(rgba.data(), 4, 2, false, false, mips);
    TEST_CHECK(mips[2][0] == 128);
    std::vector<u8> normals = {255, 128, 128, 255, 0, 128, 128, 255}; // +x, -x → average points toward z
    render::buildMips(normals.data(), 2, 1, false, true, mips);
    TEST_CHECK(mips.size() == 2 && mips[1][0] == 128);
    TEST_CHECK(render::packHalf2(1.0f, 0.5f) == (0x3C00u | (0x3800u << 16)) && render::packHalf2(0.0f, -2.0f) == (0xC000u << 16));
}

// Asset budgets: kind is the first folder under models/, only exceeded items as sentences
TEST_CASE(asset, asset_budget) {
    TEST_CHECK(asset::modelKind("models/characters/hero.glb") == asset::ModelKind::Character);
    TEST_CHECK(asset::modelKind("models/environment/cliff.glb") == asset::ModelKind::Environment);
    TEST_CHECK(asset::modelKind("models/tree.glb") == asset::ModelKind::Prop);
    TEST_CHECK(asset::modelKind("models\\environment\\rock.glb") == asset::ModelKind::Environment); // backslashes
    TEST_CHECK(asset::modelKind("models/other/x.glb") == asset::ModelKind::Prop);
    asset::Model m;
    m.triangles[0] = 3200;
    m.materials = 2;
    const std::vector<std::string> over = asset::budgetProblems(m, asset::kDefaultBudgets[1]);
    TEST_CHECK(over.size() == 1 && over[0] == "3200 triangles (budget 3000)");
    TEST_CHECK(asset::budgetProblems(m, asset::kDefaultBudgets[0]).empty());
}

// Frustum culling: camera (0,0,-10) looking at the origin (reversed-Z, near plane 0.1, far plane 100)
TEST_CASE(render, frustum_culling) {
    const Mat4 vp = lookAtLH({0, 0, -10}, {0, 0, 0}, {0, 1, 0}) * perspectiveReversedZ(1.0f, 1.0f, 0.1f, 100.0f);
    const render::Frustum f = render::Frustum::fromViewProj(vp);
    TEST_CHECK(f.count == 6);
    TEST_CHECK(f.sphereVisible({0, 0, 0}, 1.0f));
    TEST_CHECK(!f.sphereVisible({100, 0, 0}, 1.0f));  // to the side
    TEST_CHECK(!f.sphereVisible({0, 0, -20}, 1.0f));  // behind
    TEST_CHECK(!f.sphereVisible({0, 0, 200}, 1.0f));  // beyond the far plane
    TEST_CHECK(f.sphereVisible({0, 0, 200}, 120.0f)); // straddles the far plane
    TEST_CHECK(f.sphereVisible({8, 0, 0}, 3.0f));     // straddles a side plane (screen edge about 5.5)
    const Vec3 pts[] = {{-1, 0, 0}, {3, 0, 0}, {1, 2, 0}};
    const render::BoundingSphere b = render::boundingSphere(pts, 3);
    TEST_CHECK(std::abs(b.center.x - 1.0f) < 1e-6f && std::abs(b.center.y - 1.0f) < 1e-6f && std::abs(b.radius - std::sqrt(5.0f)) < 1e-5f);
}

// Performance numbers: average/worst frame per report period, missing sources drop out of the text
TEST_CASE(game, frame_stats) {
    game::FrameStats perf(0.25);
    TEST_CHECK(!perf.update(0.1, {}) && !perf.update(0.1, {}));
    TEST_CHECK(perf.update(0.1, {})); // 0.3 s: end of period
    const game::FrameSample& s = perf.sample();
    TEST_CHECK(s.frames == 3 && std::abs(s.frameMs - 100.0) < 1e-6 && std::abs(s.worstFrameMs - 100.0) < 1e-6);
    TEST_CHECK(std::abs(s.fps - 10.0) < 1e-6 && !s.hasGame && !s.hasSession && !s.hasRenderer && !s.hasAudio);
    TEST_CHECK(perf.line() == "10 fps 100.00 ms (worst 100.0)" && perf.lines() == perf.line());
    TEST_CHECK(!perf.update(0.05, {}) && perf.update(0.4, {})); // new period: the previous period's values don't remain
    TEST_CHECK(perf.sample().frames == 2 && std::abs(perf.sample().worstFrameMs - 400.0) < 1e-6);
    // 1% worst frames: nearest rank (the 2 longest of 200 frames are 1%)
    std::vector<f64> times(200, 0.010);
    times[7] = 0.050;
    times[90] = 0.040;
    TEST_CHECK(std::abs(game::frameMsPercentile(times, 99.0) - 10.0) < 1e-6);
    times[3] = 0.030;
    TEST_CHECK(std::abs(game::frameMsPercentile(times, 99.0) - 30.0) < 1e-6);
    std::vector<f64> none;
    TEST_CHECK(game::frameMsPercentile(none, 99.0) == 0.0);
    // Live connection "stats" topic: batches from missing sources are omitted
    const std::string json = perf.json();
    TEST_CHECK(json.rfind("{\"seconds\":", 0) == 0 && json.find("\"frames\":2") != std::string::npos && json.back() == '}');
    TEST_CHECK(json.find("\"game\"") == std::string::npos && json.find("\"render\"") == std::string::npos);
}

// Live connection (mealkit/live/Server.h): connection info file, token paths, Origin, service paths (socket round trips are live_probe in check.ps1)
TEST_CASE(live, live_server) {
    const std::string project = executableDirectory() + "/live_test";
    live::Server server;
    live::ServerConfig c;
    c.kind = "game";
    c.projectDir = project;
    c.projectName = "Test";
    c.token = "tok";
    TEST_CHECK(server.start(c) && server.port() != 0 && server.baseUrl() == server.origin() + "/tok/");
    std::string info;
    TEST_CHECK(readTextFile(server.infoPath(), info) && info.find("\"token\":\"tok\"") != std::string::npos &&
               info.find("\"port\":" + std::to_string(server.port())) != std::string::npos && info.find("\"protocol\":1") != std::string::npos);

    live::Response r = server.handle("GET", "/tok/api/hello", "", "");
    TEST_CHECK(r.status == 200 && r.body.find("\"kind\":\"game\"") != std::string::npos && r.body.find("\"project\":\"Test\"") != std::string::npos);
    TEST_CHECK(server.handle("GET", "/other/api/hello", "", "").status == 404);
    TEST_CHECK(server.handle("GET", "/tok/api/hello", "https://example.com", "").status == 403); // web pages can't call it
    TEST_CHECK(server.handle("GET", "/tok/api/hello", server.origin(), "").status == 200);

    live::Service echo;
    echo.name = "echo";
    echo.allowOrigin = [](const std::string& origin) { return origin == "https://design.penpot.app"; };
    echo.handle = [](const live::Request& req) {
        live::Response out;
        out.body = req.method + " " + req.path + " " + req.query + " " + req.body;
        return out;
    };
    server.addService(echo);
    std::string allow;
    r = server.handle("POST", "/tok/echo/a/b?x=1", "https://design.penpot.app", "hi", &allow);
    TEST_CHECK(r.status == 200 && r.body == "POST a/b x=1 hi" && allow == "https://design.penpot.app");
    TEST_CHECK(server.handle("GET", "/tok/echo/a", "https://example.com", "").status == 403);
    TEST_CHECK(server.handle("GET", "/tok/api/hello", "", "").body.find("\"services\":[\"echo\"]") != std::string::npos);
    TEST_CHECK(!server.listening("stats"));
    server.publish("stats", "{}"); // fine without listeners. Goes into hello's topic list
    TEST_CHECK(server.handle("GET", "/tok/api/hello", "", "").body.find("\"topics\":[\"stats\"]") != std::string::npos);

    const std::string path = server.infoPath();
    server.stop();
    TEST_CHECK(!fileExists(path));
    // Files left by abnormal exits (missing pid) are deleted by the next server
    TEST_CHECK(writeTextFile(project + "/build/live/game-4000000000.json", "{}"));
    TEST_CHECK(server.start(c));
    TEST_CHECK(!fileExists(project + "/build/live/game-4000000000.json"));
    server.stop();
}
#endif

MEALKIT_STRUCT(TestCommand) {
    MEALKIT_FIELD(u32, value, 0, "값");
    MEALKIT_FIELD(Vec3, where, {}, "위치");
};

MEALKIT_STRUCT(OtherCommand) {
    MEALKIT_FIELD(u8, flag, 0, "표시");
};

// Collects commands per confirmed tick of a session: "tick player value" lines
struct CommandLog {
    std::vector<std::string> lines;
    void collect(u32 tick, const net::TickInputs& in) {
        sim::forEachCommand(in, [&](const sim::Command& c) {
            if (TestCommand t; c.read(t)) lines.push_back(std::to_string(tick) + " " + std::to_string(c.player) + " " + std::to_string(t.value));
            else if (c.is<OtherCommand>()) lines.push_back(std::to_string(tick) + " " + std::to_string(c.player) + " other");
        });
    }
};

TEST_CASE(net, command_channel) {
    { // solo: sent commands go into the next confirmed tick, in send order
        net::LockstepSession s;
        TEST_CHECK(s.startOffline(1, 0));
        TestCommand a;
        a.value = 7;
        TEST_CHECK(sim::sendCommand(s, a) && sim::sendCommand(s, OtherCommand{}));
        TEST_CHECK(!s.submitCommand("x", 0) && !s.submitCommand(std::string(net::kMaxCommandBytes + 1, 'x').data(), net::kMaxCommandBytes + 1));
        s.submitLocalInput(1, {});
        CommandLog log;
        log.collect(0, s.inputs(0));
        log.collect(1, s.inputs(1));
        TEST_CHECK(log.lines.size() == 2 && log.lines[0] == "1 0 7" && log.lines[1] == "1 0 other");

        // Beyond the per-tick size, the rest go to the next tick
        for (u32 i = 0; i < 40; ++i) {
            TestCommand c;
            c.value = 100 + i;
            sim::sendCommand(s, c);
        }
        CommandLog more; // one command is 22 bytes (header 2 + type 4 + 16) → 11 per tick
        for (u32 t = 2; t < 6; ++t) {
            s.submitLocalInput(t, {});
            more.collect(t, s.inputs(t));
            TEST_CHECK(s.inputs(t).commandBytes <= net::kCommandBytesPerTick);
        }
        TEST_CHECK(more.lines.size() == 40 && more.lines.front() == "2 0 100" && more.lines.back() == "5 0 139");
        TEST_CHECK(more.lines[10] == "2 0 110" && more.lines[11] == "3 0 111");

        // Goes into the input recording too
        const std::string path = executableDirectory() + "/test_commands.rec";
        {
            game::InputRecordWriter w;
            TEST_CHECK(w.open(path, {1, 1, 0}, 0));
            for (u32 t = 0; t < 6; ++t) w.write(s.inputs(t), t);
        }
        game::InputRecording rec;
        std::string error;
        TEST_CHECK(game::loadInputRecording(path, rec, error) && rec.inputs.size() == 6);
        CommandLog replayed;
        for (u32 t = 0; t < 6; ++t) replayed.collect(t, rec.inputs[t]);
        TEST_CHECK(replayed.lines.size() == 42 && replayed.lines[0] == log.lines[0] && replayed.lines.back() == more.lines.back());
        removeFile(path);
    }
    { // host + client (UDP loopback): both see the same commands on the same tick
        constexpr u16 kPort = 27931;
        net::LockstepSession host, client;
        TEST_CHECK(host.host(kPort, 2, 2, true));
        TestCommand hostLoadout, clientLoadout; // match start data: sent in the lobby and confirmed by the start packet
        hostLoadout.value = 11;
        clientLoadout.value = 22;
        TEST_CHECK(game::setLoadout(host, hostLoadout) && game::setLoadout(client, clientLoadout));
        net::Address address;
        net::startup();
        TEST_CHECK(net::resolveAddress("127.0.0.1", kPort, address));
        net::cleanup();
        TEST_CHECK(client.join(address));
        const f64 deadline = nowSeconds() + 5.0;
        while (nowSeconds() < deadline && host.lobbyPlayerCount() < 2) {
            host.update(nowSeconds());
            client.update(nowSeconds());
        }
        host.startMatch(5, 0);
        constexpr u32 kTicks = 60;
        u32 next[2] = {0, 0};
        CommandLog logs[2];
        net::LockstepSession* sessions[2] = {&host, &client};
        while (nowSeconds() < deadline && (next[0] < kTicks || next[1] < kTicks)) {
            for (u32 i = 0; i < 2; ++i) {
                net::LockstepSession& s = *sessions[i];
                s.update(nowSeconds());
                while (next[i] < kTicks && s.canExecute(next[i])) {
                    const u32 t = next[i]++;
                    logs[i].collect(t, s.inputs(t));
                    s.tickExecuted(t, 0);
                    if (t == 5 || t == 9) { // client twice, host once
                        TestCommand c;
                        c.value = t * 10 + i;
                        if (i == 1 || t == 5) sim::sendCommand(s, c);
                    }
                    s.submitLocalInput(t + s.inputDelay(), {});
                }
            }
        }
        TEST_CHECK(next[0] == kTicks && next[1] == kTicks && !host.desynced());
        for (net::LockstepSession* s : sessions) {
            const game::MatchInfo match = game::matchInfo(*s);
            TestCommand a, b;
            TEST_CHECK(game::readLoadout(match, 0, a) && a.value == 11 && game::readLoadout(match, 1, b) && b.value == 22);
            OtherCommand wrong;
            TEST_CHECK(!game::readLoadout(match, 0, wrong)); // not read if the type differs
        }
        { // goes into the input recording header (version 3)
            const std::string path = executableDirectory() + "/test_loadouts.rec";
            {
                game::InputRecordWriter w;
                TEST_CHECK(w.open(path, game::matchInfo(host), 0));
                w.write(host.inputs(0), 0);
            }
            game::InputRecording rec;
            std::string error;
            TestCommand b;
            TEST_CHECK(game::loadInputRecording(path, rec, error) && rec.inputs.size() == 1 && game::readLoadout(rec.match, 1, b) &&
                       b.value == 22);
            removeFile(path);
        }
        TEST_CHECK(logs[0].lines.size() == 3 && logs[0].lines == logs[1].lines);
        client.shutdown();
        host.shutdown();
    }
}

// Screen flow session: lobby names/ready/loadouts, chat relay, match → lobby → second match, LAN room announcements (UDP loopback)
TEST_CASE(net, lobby_chat_rematch) {
    TEST_CHECK(net::clampUtf8("abc", 2) == "ab");
    TEST_CHECK(net::clampUtf8("\xEA\xB0\x80\xEA\xB0\x80", 4) == "\xEA\xB0\x80"); // doesn't cut in the middle of a Korean character (3 bytes)

    constexpr u16 kPort = 27933;
    net::LockstepSession host, client;
    host.setLocalName("Host");
    client.setLocalName("Guest");
    TEST_CHECK(host.host(kPort, 4, 2, true));
    TestCommand pick;
    pick.value = 7;
    TEST_CHECK(game::setLoadout(client, pick));
    net::Address address;
    net::startup();
    TEST_CHECK(net::resolveAddress("127.0.0.1", kPort, address));
    net::cleanup();
    TEST_CHECK(client.join(address));
    net::LockstepSession* sessions[2] = {&host, &client};
    auto pump = [&](auto&& done, f64 seconds) {
        const f64 until = nowSeconds() + seconds;
        while (nowSeconds() < until && !done()) {
            for (net::LockstepSession* s : sessions) s->update(nowSeconds());
            sleepSeconds(0.001);
        }
        return done();
    };
    // Lobby: the client sees its own name/ready/loadout in the host's list
    client.setLocalReady(true);
    TEST_CHECK(pump([&] {
        const auto& m = client.lobbyMembers();
        return m.size() == 2 && m[0].host && m[0].name == "Host" && m[1].name == "Guest" && m[1].ready;
    }, 5.0));
    TestCommand seen;
    TEST_CHECK(game::readLobbyLoadout(client.lobbyMembers()[1], seen) && seen.value == 7);
    // Chat: both receive in the same order (the client's own comes back via the host)
    TEST_CHECK(client.sendChat("hi") && host.sendChat("hello"));
    TEST_CHECK(!host.sendChat("too fast")); // kChatInterval
    TEST_CHECK(pump([&] { return host.chatLog().size() == 2 && client.chatLog().size() == 2; }, 5.0));
    for (net::LockstepSession* s : sessions) {
        TEST_CHECK(s->chatLog()[0].text == host.chatLog()[0].text && s->chatLog()[1].text == host.chatLog()[1].text);
        TEST_CHECK(s->chatLog()[0].text == "hello" || s->chatLog()[1].text == "hello");
    }
    TEST_CHECK(host.chatLog()[0].name == (host.chatLog()[0].text == "hi" ? "Guest" : "Host"));
    // Two matches: start → a few ticks → to the lobby (the client returns too and ready is cleared) → start again
    for (u32 match = 1; match <= 2; ++match) {
        host.startMatch(match, 0);
        TEST_CHECK(pump([&] { return client.state() == net::SessionState::Running; }, 5.0));
        TEST_CHECK(host.matchIndex() == match && client.matchIndex() == match);
        u32 next[2] = {0, 0};
        TEST_CHECK(pump([&] {
            for (u32 i = 0; i < 2; ++i)
                while (next[i] < 20 && sessions[i]->canExecute(next[i])) {
                    const u32 t = next[i]++;
                    sessions[i]->tickExecuted(t, t);
                    sessions[i]->submitLocalInput(t + sessions[i]->inputDelay(), {});
                }
            return next[0] == 20 && next[1] == 20;
        }, 5.0));
        TEST_CHECK(!host.desynced());
        host.returnToLobby();
        TEST_CHECK(pump([&] { return client.state() == net::SessionState::Lobby; }, 5.0));
        TEST_CHECK(!client.localReady());
        client.setLocalReady(true);
        TEST_CHECK(pump([&] { return host.lobbyMembers().size() == 2 && host.lobbyMembers()[1].ready; }, 5.0));
    }
    client.reset();
    TEST_CHECK(client.state() == net::SessionState::Idle && client.localName() == "Guest");
    TEST_CHECK(pump([&] { return host.lobbyPlayerCount() == 1; }, 5.0)); // people who left drop out of the lobby
    host.shutdown();

    // LAN room announcements: found even when sent only to 127.0.0.1, other game names are ignored
    net::RoomFinder finder, other;
    TEST_CHECK(finder.open("TestGame") && other.open("OtherGame"));
    net::RoomAnnouncer announcer;
    TEST_CHECK(announcer.open(false));
    net::RoomAnnouncement room;
    room.game = "TestGame";
    room.hostName = "Host";
    room.port = 12345;
    room.players = 1;
    room.maxPlayers = 4;
    const f64 until = nowSeconds() + 3.0;
    while (nowSeconds() < until && finder.rooms().empty()) {
        announcer.update(nowSeconds(), room);
        finder.update(nowSeconds());
        other.update(nowSeconds());
        sleepSeconds(0.01);
    }
    TEST_CHECK(finder.rooms().size() == 1 && finder.rooms()[0].hostName == "Host" &&
               finder.rooms()[0].address.port == 12345 && other.rooms().empty());
}

// Network condition simulation: even with delay/jitter/loss, lockstep reaches the end with the same inputs (retransmit/redundant send paths)
TEST_CASE(net, net_conditions) {
    constexpr u16 kPort = 27935;
    net::LockstepSession host, client;
    const net::NetConditions bad{60.0f, 30.0f, 15.0f};
    host.setConditions(bad);
    client.setConditions(bad);
    TEST_CHECK(host.host(kPort, 2, 3, true));
    net::Address address;
    net::startup();
    TEST_CHECK(net::resolveAddress("127.0.0.1", kPort, address));
    net::cleanup();
    TEST_CHECK(client.join(address));
    net::LockstepSession* sessions[2] = {&host, &client};
    const f64 deadline = nowSeconds() + 15.0;
    while (nowSeconds() < deadline && host.lobbyPlayerCount() < 2) {
        for (net::LockstepSession* s : sessions) s->update(nowSeconds());
        sleepSeconds(0.001);
    }
    host.startMatch(9, 0);
    constexpr u32 kTicks = 90;
    u32 next[2] = {0, 0};
    u64 sum[2] = {0, 0};
    while (nowSeconds() < deadline && (next[0] < kTicks || next[1] < kTicks)) {
        for (u32 i = 0; i < 2; ++i) {
            net::LockstepSession& s = *sessions[i];
            s.update(nowSeconds());
            while (next[i] < kTicks && s.canExecute(next[i])) {
                const u32 t = next[i]++;
                for (u32 p = 0; p < s.playerCount(); ++p) sum[i] = sum[i] * 31 + s.inputs(t).players[p].bytes[0];
                s.tickExecuted(t, sum[i]);
                net::PlayerInput in;
                in.bytes[0] = static_cast<u8>(t * 7 + i);
                s.submitLocalInput(t + s.inputDelay(), in);
            }
        }
        sleepSeconds(0.001);
    }
    TEST_CHECK(next[0] == kTicks && next[1] == kTicks && sum[0] == sum[1] && !host.desynced());
    TEST_CHECK(host.stats().rttMs == 0.0f && client.stats().rttMs > 40.0f); // the delay shows in the round trip the client measures
    TEST_CHECK(host.pingMs(1) > 40 && host.pingMs(0) == 0); // per-player round-trip time measured by the host
    // When the client stops sending input the match stalls, and both sides report waiting for player 1
    const f64 stallUntil = nowSeconds() + 3.0;
    while (nowSeconds() < stallUntil && !((host.waitingFor() & 2u) && (client.waitingFor() & 2u))) {
        for (net::LockstepSession* s : sessions) s->update(nowSeconds());
        while (host.canExecute(next[0])) {
            host.tickExecuted(next[0], 0);
            host.submitLocalInput(next[0] + host.inputDelay(), {});
            ++next[0];
        }
        sleepSeconds(0.001);
    }
    TEST_CHECK((host.waitingFor() & 2u) && !(host.waitingFor() & 1u) && (client.waitingFor() & 2u));
    client.shutdown();
    host.shutdown();
}

// Replay session: provides only recorded inputs and stops at the end of the recording. My inputs/commands are discarded, and hashes differing from the recording are reported as desyncs
TEST_CASE(net, replay_session) {
    std::vector<net::TickInputs> inputs(4);
    for (u32 t = 0; t < inputs.size(); ++t) inputs[t].players[1].bytes[0] = static_cast<u8>(10 + t);
    net::LockstepSession s;
    TEST_CHECK(s.startReplay(42, 5, {{1, 2}, {3}}, inputs, {100, 101, 102, 103}));
    TEST_CHECK(s.isReplay() && s.state() == net::SessionState::Running && s.replayTicks() == 4);
    TEST_CHECK(s.seed() == 42 && s.matchData() == 5 && s.playerCount() == 2 && s.loadout(1).size() == 1);
    TEST_CHECK(!s.submitCommand("x", 1));
    u32 t = 0;
    while (s.canExecute(t)) {
        TEST_CHECK(s.inputs(t).players[1].bytes[0] == 10 + t);
        s.submitLocalInput(t + s.inputDelay(), {});
        s.tickExecuted(t, t == 2 ? 999 : 100 + t);
        ++t;
    }
    TEST_CHECK(t == 4 && s.desynced() && s.desyncTick() == 2);
    TEST_CHECK(!s.startReplay(1, 0, {{}}, {}, {})); // can't open while Running
}

// Changing input delay: even when increased and decreased mid-match, both peers change from the same tick and reach the end with the same inputs (filling empty ticks, skipping overlapping ticks)
TEST_CASE(net, input_delay_change) {
    net::AutoDelay a;
    a.tickMs = 1000.0f / 30.0f;
    a.minTicks = 2;
    a.maxTicks = 8;
    TEST_CHECK(net::delayForRtt(0.0f, a) == 2 && net::delayForRtt(100.0f, a) == 5 && net::delayForRtt(2000.0f, a) == 8);

    constexpr u16 kPort = 27937;
    net::LockstepSession host, client;
    TEST_CHECK(host.host(kPort, 2, 3, true));
    net::Address address;
    net::startup();
    TEST_CHECK(net::resolveAddress("127.0.0.1", kPort, address));
    net::cleanup();
    TEST_CHECK(client.join(address));
    net::LockstepSession* sessions[2] = {&host, &client};
    const f64 deadline = nowSeconds() + 15.0;
    while (nowSeconds() < deadline && host.lobbyPlayerCount() < 2) {
        for (net::LockstepSession* s : sessions) s->update(nowSeconds());
        sleepSeconds(0.001);
    }
    host.startMatch(3, 0);
    constexpr u32 kTicks = 150;
    u32 next[2] = {0, 0};
    u64 sum[2] = {0, 0};
    u32 delayAt[2][kTicks] = {};
    bool raised = false, lowered = false;
    while (nowSeconds() < deadline && (next[0] < kTicks || next[1] < kTicks)) {
        for (u32 i = 0; i < 2; ++i) {
            net::LockstepSession& s = *sessions[i];
            s.update(nowSeconds());
            while (next[i] < kTicks && s.canExecute(next[i])) {
                const u32 t = next[i]++;
                for (u32 p = 0; p < s.playerCount(); ++p) sum[i] = sum[i] * 31 + s.inputs(t).players[p].bytes[0] + 1;
                s.tickExecuted(t, sum[i]);
                delayAt[i][t] = s.inputDelay();
                net::PlayerInput in;
                in.bytes[0] = static_cast<u8>(t * 3 + i * 101);
                s.submitLocalInput(t + s.inputDelay(), in);
            }
        }
        if (!raised && next[0] >= 30) raised = host.requestInputDelay(7);
        if (!lowered && next[0] >= 80) lowered = host.requestInputDelay(2);
        sleepSeconds(0.001);
    }
    TEST_CHECK(raised && lowered && next[0] == kTicks && next[1] == kTicks && sum[0] == sum[1] && !host.desynced());
    TEST_CHECK(host.inputDelay() == 2 && client.inputDelay() == 2);
    bool sameSwitch = true, sawSeven = false;
    for (u32 t = 0; t < kTicks; ++t) {
        sameSwitch = sameSwitch && delayAt[0][t] == delayAt[1][t];
        sawSeven = sawSeven || delayAt[0][t] == 7;
    }
    TEST_CHECK(sameSwitch && sawSeven); // everyone changed on the same tick
    client.shutdown();
    host.shutdown();
}

// Connection rejection reasons: version if the simulation version differs (with the host's game version), differing components if game data differs
TEST_CASE(net, join_reject_reasons) {
    constexpr u16 kPort = 27939;
    net::Address address;
    net::startup();
    TEST_CHECK(net::resolveAddress("127.0.0.1", kPort, address));
    net::cleanup();
    auto tryJoin = [&](u64 clientSim, const u64* clientParts, u64 clientContent) {
        net::LockstepSession host, client;
        const u64 hostParts[5] = {1, 2, 3, 4, 5};
        host.setBuild(0xABCull, "1.3");
        host.setContentParts(hostParts);
        host.setContentHash(100);
        client.setBuild(clientSim, "1.2");
        client.setContentParts(clientParts);
        client.setContentHash(clientContent);
        TEST_CHECK(host.host(kPort, 2, 3, true) && client.join(address));
        const f64 until = nowSeconds() + 5.0;
        while (nowSeconds() < until && client.state() == net::SessionState::Connecting) {
            host.update(nowSeconds());
            client.update(nowSeconds());
            sleepSeconds(0.001);
        }
        net::RejectInfo info = client.rejectInfo();
        client.shutdown();
        host.shutdown();
        return info;
    };
    const u64 same[5] = {1, 2, 3, 4, 5};
    const u64 otherTuning[5] = {1, 2, 9, 4, 8};
    net::RejectInfo r = tryJoin(0xDEFull, same, 100);
    TEST_CHECK(r.reason == net::RejectReason::Version && r.hostVersion == "1.3");
    r = tryJoin(0xABCull, otherTuning, 101);
    TEST_CHECK(r.reason == net::RejectReason::Content &&
               r.contentMask == ((1u << static_cast<u32>(net::ContentPart::Tuning)) | (1u << static_cast<u32>(net::ContentPart::Tables))));
    r = tryJoin(0xABCull, same, 100);
    TEST_CHECK(r.reason == net::RejectReason::None); // joins if identical
}

// Crossplay: input kinds travel in lobby signals, a same-input-only host rejects other kinds, and crossplay off still meets the same OS
TEST_CASE(net, join_input_kind_crossplay) {
    constexpr u16 kPort = 27941;
    net::Address address;
    net::startup();
    TEST_CHECK(net::resolveAddress("127.0.0.1", kPort, address));
    net::cleanup();
    auto run = [&](bool sameInputOnly, net::InputKind clientInput, bool clientCrossplay, net::InputKind& seenByClient,
                   net::InputKind& seenByHost) {
        net::LockstepSession host, client;
        host.setSameInputOnly(sameInputOnly);
        host.setLocalInputKind(net::InputKind::Gamepad);
        client.setLocalInputKind(clientInput);
        client.setCrossplay(clientCrossplay);
        TEST_CHECK(host.host(kPort, 2, 3, true) && client.join(address));
        const f64 until = nowSeconds() + 5.0;
        while (nowSeconds() < until && client.state() != net::SessionState::Ended &&
               !(client.lobbyMembers().size() == 2 && host.lobbyMembers().size() == 2)) {
            host.update(nowSeconds());
            client.update(nowSeconds());
            sleepSeconds(0.001);
        }
        if (client.lobbyMembers().size() == 2) seenByClient = client.lobbyMembers()[0].input;
        if (host.lobbyMembers().size() == 2) seenByHost = host.lobbyMembers()[1].input;
        net::RejectInfo info = client.rejectInfo();
        client.shutdown();
        host.shutdown();
        return info;
    };
    net::InputKind byClient = net::InputKind::KeyboardMouse, byHost = net::InputKind::KeyboardMouse;
    net::RejectInfo r = run(false, net::InputKind::Touch, false, byClient, byHost);
    TEST_CHECK(r.reason == net::RejectReason::None); // crossplay off, same OS: joins
    TEST_CHECK(byClient == net::InputKind::Gamepad && byHost == net::InputKind::Touch);
    r = run(true, net::InputKind::KeyboardMouse, true, byClient, byHost);
    TEST_CHECK(r.reason == net::RejectReason::Input && r.hostInput == net::InputKind::Gamepad);
    r = run(true, net::InputKind::Gamepad, true, byClient, byHost);
    TEST_CHECK(r.reason == net::RejectReason::None);
}

TEST_CASE(game, project_file) {
    const std::string path = executableDirectory() + "\\test.project";
    // Reads even with the old format (key=value) and new format (path value) mixed
    TEST_CHECK(writeTextFile(path, "# header line\r\nname=Old\r\nengine = D:\\My Engine\r\n"
                                   "scene scenes/b.scene\r\ngame.port 99\r\nfutureKey 1 2\r\n"));
    game::ProjectFile p;
    TEST_CHECK(game::loadProjectFile(path, p));
    TEST_CHECK(p.settings.name == "Old" && p.settings.engine == "D:\\My Engine" && p.settings.scene == "scenes/b.scene");
    TEST_CHECK(p.gameFields.size() == 1 && p.gameFields[0].first == "port" && p.gameFields[0].second == "99");
    TEST_CHECK(p.unknownFields.size() == 1 && p.unknownFields[0].second == "1 2");
    TEST_CHECK(p.header == "# header line\n");
    TEST_CHECK(game::startSceneAsset(p.settings) == "assets/scenes/b.scene");

    // Saving writes the new format, rereading gives the same values
    TEST_CHECK(game::saveProjectFile(path, p));
    std::string saved;
    TEST_CHECK(readTextFile(path, saved) && saved.find("engine D:\\My Engine\n") != std::string::npos &&
               saved.find("game.port 99\n") != std::string::npos);
    game::ProjectFile q;
    TEST_CHECK(game::loadProjectFile(path, q));
    TEST_CHECK(q.settings.name == "Old" && q.settings.engine == p.settings.engine && q.gameFields == p.gameFields &&
               q.unknownFields == p.unknownFields && q.header == p.header);
    removeFile(path);

    // All engine items are hash-excluded: only hash targets go into the content hash
    game::ProjectSettings other = q.settings;
    other.engine = "E:\\elsewhere";
    TEST_CHECK(reflect::hashOf(q.settings, reflect::kFieldView) == reflect::hashOf(other, reflect::kFieldView));
}

// Format line "@format <kind> <N>" (mealkit/core/TextFormat.h, contract between tools)
TEST_CASE(core, text_format) {
    TEST_CHECK(text::formatLine(text::kTable) == "@format table 1");
    u32 version = 0;
    TEST_CHECK(!text::readFormatLine("row a", text::kTable, "t", version) && version == 0);
    TEST_CHECK(text::readFormatLine("@format table 1", text::kTable, "t", version) && version == 1);
    TEST_CHECK(text::readFormatLine("@format table 7", text::kTable, "t", version) && version == 7); // newer version: warning
    version = 0;
    TEST_CHECK(text::readFormatLine("@format", text::kTable, "t", version) && version == 0); // malformed line: warn and skip

    // Writers put the format line after the header comment, and readers don't read that line as a value
    const std::string dir = executableDirectory();
    data::TableText table;
    table.ids = {"a"};
    table.rows = {{{"weight", "2"}}};
    TEST_CHECK(data::writeTableText(dir + "/test_format.table", table, "TestKind"));
    std::string body;
    TEST_CHECK(readTextFile(dir + "/test_format.table", body));
    TEST_CHECK(body.find("\n@format table 1\n") != std::string::npos && body.rfind("# MealKit table", 0) == 0);
    data::TableData data;
    std::string warnings;
    TEST_CHECK(data::loadTableData(reflect::typeOf<TestKind>(), dir + "/test_format.table", data, &warnings) && warnings.empty());
    TEST_CHECK(data.ids.size() == 1 && static_cast<const TestKind*>(data.rows[0].data())->weight == 2.0f);
    removeFile(dir + "/test_format.table");

    TestKind kind;
    kind.weight = 3.5f;
    TEST_CHECK(reflect::saveTextFile(reflect::typeOf<TestKind>(), &kind, dir + "/test_format.tuning", text::kTuning, "test"));
    TEST_CHECK(readTextFile(dir + "/test_format.tuning", body) && body.rfind("# test\n@format tuning 1\n", 0) == 0);
    TestKind back;
    TEST_CHECK(reflect::loadTextFile(reflect::typeOf<TestKind>(), &back, dir + "/test_format.tuning", text::kTuning) && back.weight == 3.5f);
    removeFile(dir + "/test_format.tuning");

    // Old files without a format line read as version 1; the project file header gets no format line
    const std::string project = dir + "/test_format.project";
    TEST_CHECK(writeTextFile(project, "# head\nname Old\n"));
    game::ProjectFile p;
    TEST_CHECK(game::loadProjectFile(project, p) && p.settings.name == "Old");
    TEST_CHECK(game::saveProjectFile(project, p) && readTextFile(project, body) && body.rfind("# head\n@format project 1\n", 0) == 0);
    game::ProjectFile q;
    TEST_CHECK(game::loadProjectFile(project, q) && q.header == "# head\n" && q.unknownFields.empty());
    removeFile(project);
}

// Leaves a hash every tick doing the same operations. At tick 30, deletes one body and creates a new one (whether restore also reverts body list changes)
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

TEST_CASE(jobs, jobs_parallel_for) {
    // Without a pool everything runs inline; with one each index is visited exactly once
    std::vector<u32> hits(10000, 0);
    const auto visit = [&](u32 begin, u32 end) {
        for (u32 i = begin; i < end; ++i) hits[i] += 1;
    };
    jobs::parallelFor(static_cast<u32>(hits.size()), 64, visit);
    jobs::acquire();
    TEST_CHECK(jobs::workerCount() >= 1);
    jobs::parallelFor(static_cast<u32>(hits.size()), 64, visit);
    jobs::parallelFor(3, 64, visit); // smaller than one chunk: inline
    bool ok = true;
    for (size_t i = 0; i < hits.size(); ++i) ok = ok && hits[i] == (i < 3 ? 3u : 2u);
    TEST_CHECK(ok);
    std::atomic<u32> done{0};
    {
        jobs::Task task = jobs::run([&] { done = 7; });
        task.wait();
        TEST_CHECK(task.isReady() && done == 7);
    } // Tasks end before the pool's last release
    jobs::release();
    TEST_CHECK(jobs::workerCount() == 0);
}

TEST_CASE(physics, physics_snapshot) {
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
    const std::vector<u64> first = runPhysics(w, bodies, 40, 20); // bodies change at tick 30
    const physics::BodyId newBody = bodies[3];

    TEST_CHECK(w.restore(snap));
    bodies = savedBodies;
    const std::vector<u64> second = runPhysics(w, bodies, 40, 20);
    TEST_CHECK(first == second);
    TEST_CHECK(bodies[3] == newBody); // the recreated body's id is the same too
    TEST_CHECK(w.bodyCount() == 61);
    std::printf("  physics snapshot: %zu bytes for 61 bodies\n", snap.byteSize());
}

// Late join: a snapshot written as bytes gives the same result even when cleared into and loaded in another World (with other bodies)
// 2D games (PhysicsWorld::setPlane2D): bodies knocked sideways and spun stay on z = 0 and turn only about z; characters too
TEST_CASE(physics, physics_plane2d) {
    physics::PhysicsWorld w;
    w.init(256, 1);
    w.setPlane2D(true);
    w.setGravity({0.0f, -20.0f, 0.0f});
    physics::BodyDesc ground;
    ground.motion = physics::Motion::Static;
    ground.position = {0.0f, -0.5f, 0.0f};
    w.createBox({20.0f, 0.5f, 20.0f}, ground);
    physics::BodyDesc slope; // tilted about x: would slide boxes toward -z/+z in 3D
    slope.motion = physics::Motion::Static;
    slope.position = {0.0f, 1.0f, 0.0f};
    slope.rotation = quatFromAxisAngle({1.0f, 0.0f, 0.0f}, 0.5f);
    w.createBox({2.0f, 0.2f, 2.0f}, slope);
    physics::BodyDesc d;
    d.position = {0.3f, 4.0f, 0.0f};
    const physics::BodyId box = w.createBox({0.3f, 0.3f, 0.3f}, d);
    w.setLinearVelocity(box, {2.0f, 0.0f, 5.0f});
    physics::CharacterDesc cd;
    cd.position = {-4.0f, 1.0f, 0.0f};
    const physics::CharacterId character = w.createCharacter(cd);
    for (int i = 0; i < 120; ++i) {
        w.moveCharacter(character, {1.0f, 0.0f, 3.0f}, {0.0f, -20.0f, 0.0f}, 1.0f / 30.0f, 0.3f, 0.3f);
        w.step(1.0f / 30.0f);
    }
    TEST_CHECK(std::abs(w.position(box).z) < 1e-4f);
    const Quat q = w.rotation(box); // only about z: x and y parts stay 0
    TEST_CHECK(std::abs(q.x) < 1e-4f && std::abs(q.y) < 1e-4f);
    TEST_CHECK(std::abs(w.characterPosition(character).z) < 1e-4f);
    TEST_CHECK(w.characterPosition(character).x > -3.0f); // it still moved along x
}

TEST_CASE(physics, physics_snapshot_bytes) {
    auto build = [](physics::PhysicsWorld& w, int count, std::vector<physics::BodyId>& bodies) {
        w.init(1024, 2);
        w.setGravity({0.0f, -20.0f, 0.0f});
        physics::BodyDesc ground;
        ground.motion = physics::Motion::Static;
        ground.position = {0.0f, -0.5f, 0.0f};
        w.createBox({20.0f, 0.5f, 20.0f}, ground);
        Pcg32 rng(9);
        for (int i = 0; i < count; ++i) {
            physics::BodyDesc d;
            d.position = {rng.range(-3.0f, 3.0f), rng.range(1.0f, 10.0f), rng.range(-3.0f, 3.0f)};
            bodies.push_back(i % 2 ? w.createSphere(0.3f, d) : w.createCapsule(0.3f, 0.2f, d));
        }
    };
    physics::PhysicsWorld host, joiner;
    std::vector<physics::BodyId> hostBodies, joinerBodies;
    build(host, 40, hostBodies);
    build(joiner, 7, joinerBodies); // different shapes for the same ids
    runPhysics(host, hostBodies, 20, 0);

    physics::PhysicsSnapshot snap, received;
    host.save(snap);
    std::vector<u8> bytes;
    snap.write(bytes);
    TEST_CHECK(received.read(bytes.data(), bytes.size()));
    TEST_CHECK(!received.read(bytes.data(), bytes.size() / 2)); // truncated data
    joiner.clearBodies();
    TEST_CHECK(joiner.restore(received));
    joinerBodies = hostBodies;
    const std::vector<u64> a = runPhysics(host, hostBodies, 40, 20);
    const std::vector<u64> b = runPhysics(joiner, joinerBodies, 40, 20);
    TEST_CHECK(a == b);
    std::printf("  physics snapshot bytes: %zu for 41 bodies\n", bytes.size());
}

// Movement modes: each mode walks on the ground, the kinematic one climbs a step, and a snapshot as bytes
// (with a character created and destroyed after it) restores the same per-tick hashes in another world
TEST_CASE(sim, movement_modes) {
    struct Run {
        physics::PhysicsWorld w;
        sim::Mover movers[3];
    };
    const sim::MoveTuning tuning;
    auto build = [&](Run& r) {
        r.w.init(1024, 2);
        r.w.setGravity({0.0f, -20.0f, 0.0f});
        physics::BodyDesc ground;
        ground.motion = physics::Motion::Static;
        ground.position = {0.0f, -0.5f, 0.0f};
        r.w.createBox({30.0f, 0.5f, 30.0f}, ground);
        ground.position = {0.0f, 0.15f, 6.0f}; // a 0.3 m step across every lane
        r.w.createBox({30.0f, 0.15f, 1.0f}, ground);
        for (u32 i = 0; i < 3; ++i) {
            sim::MoverShape shape;
            shape.position = {static_cast<f32>(i) * 3.0f, 1.0f, 0.0f};
            r.movers[i] = sim::createMover(r.w, static_cast<sim::MoveMode>(i), shape, tuning);
        }
    };
    auto run = [&](Run& r, int from, int ticks) {
        std::vector<u64> hashes;
        for (int t = from; t < from + ticks; ++t) {
            sim::MoveIntent intent;
            intent.wish = {0.0f, 0.0f, t >= 60 && t < 120 ? 1.0f : 0.0f}; // stand, walk for 2 s, stand onto the step
            intent.jump = t == 40;
            for (sim::Mover& m : r.movers) sim::move(r.w, m, intent, tuning, 1.0f, {0.0f, -20.0f, 0.0f}, 1.0f / 30.0f);
            r.w.step(1.0f / 30.0f, 2);
            Vec3 p, v;
            for (sim::Mover& m : r.movers) sim::senseMover(r.w, m, tuning, p, v);
            Hasher h;
            r.w.hashState(h);
            hashes.push_back(h.value);
        }
        return hashes;
    };
    Run a;
    build(a);
    run(a, 0, 35);
    Vec3 p, v;
    for (sim::Mover& m : a.movers) {
        sim::senseMover(a.w, m, tuning, p, v);
        TEST_CHECK(m.grounded == 1u);
        TEST_CHECK(p.y > 0.8f && p.y < 1.5f); // standing, not sunk or flying (floating hovers a little)
    }
    physics::PhysicsSnapshot snap, received;
    a.w.save(snap);
    std::vector<u8> bytes;
    snap.write(bytes);
    TEST_CHECK(received.read(bytes.data(), bytes.size()));

    const std::vector<u64> first = run(a, 35, 60);
    sim::senseMover(a.w, a.movers[0], tuning, p, v);
    TEST_CHECK(p.z > 6.0f && p.y > 1.1f); // kinematic walked up onto the step
    Run b;
    build(b);
    sim::Mover extra = sim::createMover(b.w, sim::MoveMode::Kinematic, {}, tuning); // different state before loading
    sim::destroyMover(b.w, extra);
    b.w.clearBodies();
    TEST_CHECK(b.w.restore(received));
    // Movers' own fields (grounded, gap) are game state that the game's snapshot carries: take them from a second run at tick 35
    Run c;
    build(c);
    run(c, 0, 35);
    for (u32 i = 0; i < 3; ++i) b.movers[i] = c.movers[i];
    TEST_CHECK(run(b, 35, 60) == first);
}

// Active ragdoll: 4 ragdolls stand up, walk, get knocked down and get up again; a snapshot as bytes (with a ragdoll
// created and destroyed after it, which reorders joints) restores the same per-tick hashes, and thread count doesn't matter
TEST_CASE(sim, ragdoll_movement) {
    struct Run {
        physics::PhysicsWorld w;
        sim::Mover movers[4];
    };
    const sim::MoveTuning tuning;
    const Vec3 gravity{0.0f, -20.0f, 0.0f};
    const f32 dt = 1.0f / 30.0f;
    auto build = [&](Run& r, u32 threads) {
        r.w.init(1024, threads);
        r.w.setGravity(gravity);
        physics::BodyDesc ground;
        ground.motion = physics::Motion::Static;
        ground.position = {0.0f, -0.5f, 0.0f};
        r.w.createBox({30.0f, 0.5f, 30.0f}, ground);
        for (u32 i = 0; i < 4; ++i) {
            sim::MoverShape shape;
            shape.position = {static_cast<f32>(i) * 2.0f, 0.95f, 0.0f};
            shape.group = i + 1;
            r.movers[i] = sim::createMover(r.w, sim::MoveMode::Ragdoll, shape, tuning);
        }
    };
    f64 stepSeconds = 0.0;
    auto run = [&](Run& r, int from, int ticks) {
        std::vector<u64> hashes;
        for (int t = from; t < from + ticks; ++t) {
            sim::MoveIntent intent;
            intent.wish = {0.0f, 0.0f, t >= 60 && t < 120 ? 1.0f : 0.0f}; // stand, walk for 2 s, stand
            if (t == 150) sim::knockRagdoll(r.w, r.movers[1], 30, {6.0f, 4.0f, 0.0f});
            const f64 start = nowSeconds();
            for (sim::Mover& m : r.movers) sim::move(r.w, m, intent, tuning, 1.0f, gravity, dt);
            r.w.step(dt, 2);
            Vec3 p, v;
            for (sim::Mover& m : r.movers) sim::senseMover(r.w, m, tuning, p, v);
            stepSeconds += nowSeconds() - start;
            Hasher h;
            r.w.hashState(h);
            hashes.push_back(h.value);
        }
        return hashes;
    };
    auto upright = [](const physics::PhysicsWorld& w, const sim::Mover& m) {
        return rotate(w.rotation(m.body), Vec3{0.0f, 1.0f, 0.0f}).y; // 1 = pelvis level
    };
    Run a;
    build(a, 2);
    TEST_CHECK(a.w.jointCount() == 4 * sim::kRagdollJoints);
    run(a, 0, 60);
    Vec3 p, v;
    for (sim::Mover& m : a.movers) {
        sim::senseMover(a.w, m, tuning, p, v);
        TEST_CHECK(m.grounded == 1u);
        TEST_CHECK(p.y > 0.8f && p.y < 1.2f); // standing capsule center ~0.9 m
        TEST_CHECK(upright(a.w, m) > 0.95f);
    }
    run(a, 60, 90);
    sim::senseMover(a.w, a.movers[0], tuning, p, v);
    TEST_CHECK(p.z > 6.0f);                          // walked forward
    TEST_CHECK(upright(a.w, a.movers[0]) > 0.9f);
    const f64 perTick = stepSeconds / 150.0 * 1000.0;
    std::printf("  4 ragdolls (44 bodies, 40 joints): %.3f ms per tick\n", perTick);

    physics::PhysicsSnapshot snap, received;
    a.w.save(snap);
    std::vector<u8> bytes;
    snap.write(bytes);
    TEST_CHECK(received.read(bytes.data(), bytes.size()));
    const sim::Mover saved[4] = {a.movers[0], a.movers[1], a.movers[2], a.movers[3]};
    // After the snapshot: an extra ragdoll comes and goes (Jolt swap-removes its joints), then the knockdown
    sim::MoverShape extraShape;
    extraShape.position = {-4.0f, 0.95f, 0.0f};
    extraShape.group = 9;
    sim::Mover extra = sim::createMover(a.w, sim::MoveMode::Ragdoll, extraShape, tuning);
    a.w.destroyJoint(extra.ragdoll.joints[3]); // out of order on purpose
    sim::destroyMover(a.w, extra);
    TEST_CHECK(a.w.restore(snap)); // back to the snapshot in the same world
    for (u32 i = 0; i < 4; ++i) a.movers[i] = saved[i];
    const std::vector<u64> first = run(a, 150, 60);
    TEST_CHECK(a.movers[1].ragdoll.limpTicks == 0u);
    sim::senseMover(a.w, a.movers[1], tuning, p, v);
    TEST_CHECK(upright(a.w, a.movers[1]) > 0.8f); // got up again within a second after the knockdown

    Run b;
    build(b, 4); // other thread count, restored from bytes
    b.w.clearBodies();
    TEST_CHECK(b.w.restore(received));
    for (u32 i = 0; i < 4; ++i) b.movers[i] = saved[i];
    TEST_CHECK(run(b, 150, 60) == first);
}

// Contact events: the same list (sort order) even with different physics thread counts, and the same hash
std::vector<physics::ContactEvent> runContacts(u32 threads, u64& hash) {
    physics::PhysicsWorld w;
    w.init(1024, threads);
    w.setGravity({0.0f, -20.0f, 0.0f});
    w.setContactEvents(1.0f, 16);
    physics::BodyDesc ground;
    ground.motion = physics::Motion::Static;
    ground.position = {0.0f, -0.5f, 0.0f};
    w.createBox({20.0f, 0.5f, 20.0f}, ground);
    Pcg32 rng(11);
    for (int i = 0; i < 200; ++i) {
        physics::BodyDesc d;
        d.position = {rng.range(-4.0f, 4.0f), rng.range(1.0f, 12.0f), rng.range(-4.0f, 4.0f)};
        w.createBox({0.25f, 0.25f, 0.25f}, d);
    }
    std::vector<physics::ContactEvent> all;
    for (int t = 0; t < 60; ++t) {
        w.step(1.0f / 30.0f, 2);
        TEST_CHECK(w.contacts().size() <= 16);
        for (size_t i = 1; i < w.contacts().size(); ++i) TEST_CHECK(w.contacts()[i - 1].speed >= w.contacts()[i].speed);
        all.insert(all.end(), w.contacts().begin(), w.contacts().end());
    }
    Hasher h;
    w.hashState(h);
    hash = h.value;
    return all;
}

TEST_CASE(physics, physics_contact_events) {
    u64 hashOne = 0, hashFour = 0;
    const std::vector<physics::ContactEvent> one = runContacts(1, hashOne);
    const std::vector<physics::ContactEvent> four = runContacts(4, hashFour);
    TEST_CHECK(!one.empty() && one.size() == four.size() && hashOne == hashFour);
    for (size_t i = 0; i < one.size() && i < four.size(); ++i)
        TEST_CHECK(one[i].a == four[i].a && one[i].b == four[i].b && one[i].speed == four[i].speed &&
                   one[i].point.x == four[i].point.x && one[i].point.y == four[i].point.y);
    std::printf("  contact events: %zu in 60 ticks\n", one.size());
}

bool near(Vec3 a, Vec3 b) {
    return std::fabs(a.x - b.x) < 1e-4f && std::fabs(a.y - b.y) < 1e-4f && std::fabs(a.z - b.z) < 1e-4f;
}

TEST_CASE(scene, scene_hierarchy) {
    scene::Scene s;
    s.entities.reserve(8); // references returned by add() keep being used
    scene::Entity& parent = s.add(scene::kTypeSolid, "parent");
    parent.position = {10.0f, 0.0f, 0.0f};
    parent.rotation = detYawQuat(kDetHalfPi); // +x → -z ... verified by computation below
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

    // Save → load: parents kept, invalid parents become top level
    s.entities[0].parent = 99;
    s.environment.post.exposure = -0.5f; // post values too (unrelated to the content hash)
    s.environment.post.lut = "textures/grade warm.cube";
    s.environment.post.bloomTint = {1.0f, 0.5f, 0.25f};
    const std::string path = executableDirectory() + "\\test_hierarchy.scene";
    TEST_CHECK(scene::save(s, path));
    scene::Scene loaded;
    TEST_CHECK(scene::load(loaded, path));
    TEST_CHECK(loaded.entities.size() == 3 && loaded.entities[0].parent == 0 && loaded.entities[2].parent == 2);
    TEST_CHECK(loaded.environment.post.exposure == -0.5f && loaded.environment.post.lut == "textures/grade warm.cube" &&
               loaded.environment.post.bloomTint.y == 0.5f && loaded.environment.post.toneMapping);
    TEST_CHECK(loaded.contentHash() == [&] {
        s.entities[0].parent = 0;
        return s.contentHash();
    }());
    removeFile(path);
}

#if MEALKIT_TEST_FRONT // front end (bug reports, audio): not in the Linux/macOS headless build
// Audio glue: the smallest reverb zone around the listener is picked (rotated boxes too). Skipped without an output device
TEST_CASE(audio, audio_reverb_zones) {
    audio::Engine engine;
    if (!engine.init({4, true})) return;
    std::vector<audio::ReverbZone> zones(2);
    zones[0].center = {0.0f, 0.0f, 0.0f};
    zones[0].half = {20.0f, 5.0f, 20.0f};
    zones[1].center = {10.0f, 0.0f, 0.0f};
    zones[1].rotation = quatFromAxisAngle({0.0f, 1.0f, 0.0f}, 0.7853982f); // 45 degrees: a diamond seen from above
    zones[1].half = {2.0f, 2.0f, 2.0f};
    engine.setReverbZones(zones);
    const auto zoneAt = [&](Vec3 p) {
        engine.setListener(p, {0.0f, 0.0f, 1.0f});
        engine.update(1.0 / 60.0);
        return engine.spatialStats().zone;
    };
    TEST_CHECK(zoneAt({0.0f, 0.0f, 0.0f}) == "0");
    TEST_CHECK(zoneAt({10.0f, 0.0f, 2.5f}) == "1");   // inside the diamond (outside the unrotated box)
    TEST_CHECK(zoneAt({12.2f, 0.0f, 2.2f}) == "0");   // the unrotated box's corner, outside the diamond
    TEST_CHECK(zoneAt({30.0f, 0.0f, 0.0f}).empty());
    engine.resetMatch();
    TEST_CHECK(zoneAt({0.0f, 0.0f, 0.0f}).empty());
}
#endif

// Reverb zones are view-only areas: fields read back, and adding one doesn't change the content hash (multiplayer peers may differ)
TEST_CASE(scene, scene_reverb_zone) {
    scene::Scene s;
    s.add(scene::kTypeSolid, "floor");
    const u64 before = s.contentHash();
    scene::Entity& zone = s.add(scene::kTypeReverbZone, "hall");
    zone.fields = {{"roomSize", "0.9"}, {"wet", "0.5"}};
    TEST_CHECK(s.contentHash() == before);
    const reflect::TypeInfo* type = scene::findType(scene::kTypeReverbZone, nullptr);
    TEST_CHECK(type && (type->flags & reflect::kObjectArea) && (type->flags & reflect::kObjectCosmetic));
    const scene::ReverbZoneFields f = scene::readObject<scene::ReverbZoneFields>(s.entities[1]);
    TEST_CHECK(f.roomSize == 0.9f && f.wet == 0.5f && f.damping == 0.5f && f.fade == 0.8f);
    TEST_CHECK(!scene::visibleInGame(s.entities[1], nullptr));
}

TEST_CASE(scene, scene_prefab) {
    scene::Scene s;
    s.entities.reserve(8);
    s.add(scene::kTypeSolid, "other").position = {50.0f, 0.0f, 0.0f};
    scene::Entity& root = s.add(scene::kTypeSolid, "table");
    root.position = {10.0f, 1.0f, 4.0f};
    root.fields.emplace_back("hp", "3");
    scene::Entity& leg = s.add(scene::kTypeDecoration, "leg");
    leg.parent = root.id;
    leg.position = {0.5f, -0.5f, 0.0f};
    scene::Entity& lamp = s.add(scene::kTypeDecoration, "lamp");
    lamp.position = {12.0f, 2.0f, 4.0f};

    const std::string path = executableDirectory() + "\\test_prefab.prefab";
    TEST_CHECK(scene::savePrefab(s, {root.id, lamp.id}, path));
    scene::Scene prefab;
    TEST_CHECK(scene::loadPrefab(prefab, path));
    removeFile(path);
    // ids from 1, roots relative to the first root's x/z, children kept parent-relative
    TEST_CHECK(prefab.entities.size() == 3 && prefab.entities[0].id == 1 && prefab.entities[1].parent == 1 &&
               prefab.entities[2].parent == 0);
    TEST_CHECK(near(prefab.entities[0].position, {0.0f, 1.0f, 0.0f}) && near(prefab.entities[1].position, {0.5f, -0.5f, 0.0f}) &&
               near(prefab.entities[2].position, {2.0f, 2.0f, 0.0f}));
    TEST_CHECK(prefab.entities[0].fields.size() == 1 && prefab.entities[0].fields[0].second == "3");

    // Placing twice makes independent copies with new ids
    const size_t before = s.entities.size();
    const std::vector<u32> a = scene::placePrefab(s, prefab, {{-5.0f, 0.0f, 0.0f}, {}, {1.0f, 1.0f, 1.0f}});
    const std::vector<u32> b = scene::placePrefab(s, prefab, {{0.0f, 3.0f, 20.0f}, {}, {1.0f, 1.0f, 1.0f}});
    TEST_CHECK(a.size() == 2 && b.size() == 2 && s.entities.size() == before + 6 && a[0] != b[0]);
    const scene::Entity* placed = s.find(b[0]);
    TEST_CHECK(placed && near(placed->position, {0.0f, 4.0f, 20.0f}));
    const scene::Entity& placedLeg = s.entities[before + 4];
    TEST_CHECK(placedLeg.parent == b[0] && near(scene::worldTransform(s, placedLeg).position, {0.5f, 3.5f, 20.0f}));
}

// ---------------------------------------------------------------- Simulation<World> (shadow simulation, rewind)

MEALKIT_STRUCT(FakeState) {
    MEALKIT_FIELD(u32, tick, 0, "틱");
    MEALKIT_FIELD(u32, sum, 0, "입력 합");
    MEALKIT_FIELD(physics::BodyId, ball, physics::kInvalidBody, "공");
};

int g_globalCounter = 0; // deliberately created global state (the shadow simulation must catch it)

struct FakeEvent {
    u32 tick = 0;
};

struct FakeWorld {
    FakeState state;
    physics::PhysicsWorld physics;
    sim::TickEvents<FakeEvent, 4> events;
    bool useGlobal = false; // mixes in global state → determinism violation
    u32 undeclared = 0;     // undeclared state → rewind verification must catch it
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

// Runs ticks ticks with an offline session
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
    TEST_CHECK(events == sim.nextTick() - before); // events go to the view side every tick
}

TEST_CASE(scene, scene_folder_outside_hash) {
    scene::Scene a;
    a.add(scene::kTypeSolid, "Box");
    const u64 before = a.contentHash();
    a.entities[0].folder = "Props/Boxes";
    TEST_CHECK(a.contentHash() == before);
    const std::string path = executableDirectory() + "/test_folder.scene";
    TEST_CHECK(scene::save(a, path));
    scene::Scene b;
    TEST_CHECK(scene::load(b, path));
    TEST_CHECK(b.entities.size() == 1 && b.entities[0].folder == "Props/Boxes");
}

TEST_CASE(editor, fuzzy_match) {
    using editor::fuzzyScore;
    TEST_CHECK(fuzzyScore("Save Scene", "") == 0);
    TEST_CHECK(fuzzyScore("Save Scene", "svsc") > 0);
    TEST_CHECK(fuzzyScore("Save Scene", "SAVE") > fuzzyScore("Save Scene", "ave"));
    TEST_CHECK(fuzzyScore("Save Scene", "xyz") == -1);
    TEST_CHECK(fuzzyScore("Save Scene", "cs") == -1); // order matters
    TEST_CHECK(fuzzyScore("\xEC\x94\xAC \xEC\xA0\x80\xEC\x9E\xA5", "\xE3\x85\x86\xE3\x85\x88") > 0); // ssi-eun jeo-jang by initials
}

TEST_CASE(editor, undo_stack_steps) {
    editor::UndoStack<int> undo;
    undo.reset(1);
    int value = 2;
    undo.mark();
    undo.settle(true, [&] { return value; }); // still dragging: no step yet
    TEST_CHECK(!undo.canStep(true));
    undo.settle(false, [&] { return value; });
    TEST_CHECK(undo.canStep(true));
    int out = 0;
    TEST_CHECK(undo.step(true, out) && out == 1);
    TEST_CHECK(undo.step(false, out) && out == 2);
    TEST_CHECK(!undo.step(false, out));
    TEST_CHECK(undo.step(true, out) && out == 1);
    value = 3; // a new edit drops the redo side
    undo.mark();
    undo.settle(false, [&] { return value; });
    TEST_CHECK(!undo.canStep(false));
}

TEST_CASE(game, simulation_shadow_and_rewind) {
    { // normal: the shadow simulation is quiet and rewinding and rerunning matches the recording
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
        TEST_CHECK(t.recording && t.first == 300 - 2 * 150 && t.live == 400); // snapshots at ticks 0, 150, 300
        TEST_CHECK(sim.seek(37) && sim.world().state.tick == 37 && sim.timeline().viewingPast);
        TEST_CHECK(sim.seek(333) && sim.world().state.tick == 333);
        sim.stepOnce();
        TEST_CHECK(sim.world().state.tick == 334);
        TEST_CHECK(sim.replayError().empty());
        sim.setPaused(false); // back to the present
        TEST_CHECK(sim.world().hash() == liveHash && sim.world().state.sum == liveSum && !sim.timeline().viewingPast);

        sim.setPaused(true); // one tick forward from the current tick
        sim.stepOnce();
        runFake(sim, session, 401);
        TEST_CHECK(sim.nextTick() == 401);
        sim.setPaused(false);
        runFake(sim, session, 420);
        TEST_CHECK(sim.nextTick() == 420 && !sim.shadow().desynced);
    }
    { // global state → the shadow simulation catches it on the first tick and pauses
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
    { // undeclared state → rewind verification catches it
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

int main(int argc, char** argv) {
    log::setDialogs(false);
#if MEALKIT_TEST_UI
    if (argc > 1 && std::string(argv[1]) == "--write-asset-schema") // the Blender add-on's schema.json (asset_schema test)
        return writeTextFile(std::string(MEALKIT_ENGINE_DIR) + "/tools/blender-addon/mealkit/schema.json", asset::modelSchemaJson()) ? 0 : 1;
#endif
    setProjectRoot(std::string(MEALKIT_ENGINE_DIR) + "/samples/sandbox"); // sample assets the tests read (models, scenes, ui)
    std::string only;
    if (argc > 2 && std::string(argv[1]) == "--only") only = argv[2];
    return test::runAll(only);
}
