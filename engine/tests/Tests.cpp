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
#include "mealkit/game/Tables.h"
#include "mealkit/input/Actions.h"
#include "mealkit/loc/Strings.h"
#include "mealkit/core/Clock.h"
#include "mealkit/net/Lockstep.h"
#include "mealkit/net/Socket.h"
#include "mealkit/core/Reflect.h"
#include "mealkit/physics/PhysicsWorld.h"
#include "mealkit/platform/Input.h"
#include "mealkit/platform/Paths.h"
#include "mealkit/scene/Scene.h"
#include "mealkit/sim/Commands.h"
#include "mealkit/sim/Pool.h"
#include "mealkit/sim/TickEvents.h"
#include "mealkit/sim/TickTimer.h"
#if MEALKIT_TEST_UI
#include "mealkit/ui/Ui.h"
#endif

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

MEALKIT_STRUCT(Timed) {
    MEALKIT_FIELD(Ticks, cooldown, Ticks::seconds(0.5f), "재사용 대기");
    MEALKIT_FIELD(Color, tint, (Color::rgb(255, 128, 0)), "색");
};

TEST_CASE(reflect_ticks_and_color) {
    const reflect::TypeInfo& t = reflect::typeOf<Timed>();
    TEST_CHECK(t.fields[0].value->kind == reflect::Kind::Ticks && t.fields[1].value->kind == reflect::Kind::Color);
    TEST_CHECK(reflect::tickRate() == 30);
    Timed a;
    TEST_CHECK(a.cooldown.value == 15 && a.tint.value == Color::rgb(255, 128, 0).value);
    // 초 → 틱 반올림, 틱 → 초 → 틱이 같은 값
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
    // 색: #RRGGBB, #RRGGBBAA
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

TEST_CASE(reflect_enum) {
    const reflect::TypeInfo& t = reflect::typeOf<WithEnum>();
    TEST_CHECK(t.fields[0].value->enumNames.size() == 3 && t.fields[0].value->enumNames[2] == "Capsule");
    TEST_CHECK(t.fields[1].value->enumNames.empty());
    TEST_CHECK(reflect::enumName(TestShape::Capsule) == "Capsule");
    WithEnum a;
    reflect::TextFields text;
    reflect::toText(t, &a, text);
    TEST_CHECK(text[0].second == "Ball" && text[1].second == "3"); // 파일에는 이름
    TEST_CHECK(reflect::setFromText(t, &a, "shape", "Capsule") && a.shape == TestShape::Capsule);
    TEST_CHECK(reflect::setFromText(t, &a, "shape", "0") && a.shape == TestShape::Box); // 예전 숫자도 읽는다
    TEST_CHECK(!reflect::setFromText(t, &a, "shape", "Cone"));
    WithEnum b;
    b.shape = TestShape::Box;
    TEST_CHECK(reflect::hashOf(a) == reflect::hashOf(b)); // 해시는 정수 그대로
}

TEST_CASE(input_actions) {
    // 임시 프로젝트 폴더의 assets/input.bindings
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
    input::PadState pad;
    in.setKey('W', true);
    in.setKey('D', true);
    actions.update(in, pad, true, true, 0.1);
    const Vec2 m = actions.axis2("move");
    TEST_CHECK(std::fabs(length(m) - 1.0f) < 1e-5f && m.x > 0.7f && m.y > 0.7f); // 대각선도 길이 1
    actions.update(in, pad, false, true, 0.1); // 키보드를 받지 않는 동안은 놓은 상태
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
    TEST_CHECK(std::fabs(actions.axis("zoom") - 6.0f) < 1e-5f); // 키는 rate × 초, 휠은 칸 그대로

    // 게임패드: 데드존 안은 0, 밖은 0..1로 다시 편다
    in.beginFrame();
    in.clear();
    pad.connected = true;
    pad.leftX = 0.1f;
    actions.update(in, pad, true, true, 0.1);
    TEST_CHECK(length(actions.axis2("move")) == 0.0f);
    pad.leftX = 0.6f;
    pad.buttons = input::pad::A;
    actions.update(in, pad, true, true, 0.1);
    TEST_CHECK(std::fabs(actions.axis2("move").x - 0.5f) < 1e-5f && actions.pressed("jump"));
    TEST_CHECK(actions.lastDevice() == input::Device::Gamepad);
    pad.buttons = input::pad::DPadDown; // "-이름"은 음수 쪽
    actions.update(in, pad, true, true, 0.5);
    TEST_CHECK(std::fabs(actions.axis("zoom") + 5.0f) < 1e-5f);
    pad = {};

    // 플레이어가 바꾼 키: 사용자 파일에 바뀐 행만 저장하고, 다시 읽으면 프로젝트 값 위에 덮는다
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
    TEST_CHECK(again.saveUser() && !fileExists(dir + "/user.bindings")); // 모두 기본값이면 파일을 지운다

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

static_assert(reflect::hasStringField<TestItem>()); // RowRef에는 id 문자열이 있어 시뮬레이션 상태에 넣지 않는다

TEST_CASE(tables) {
    const std::string dir = executableDirectory();
    // 파일: 쓰고 읽으면 같다. 겹친 id·잘못된 id는 버린다
    game::TableText text;
    text.ids = {"plain", "heavy"};
    text.rows = {{{"weight", "1"}, {"title", "Plain, \"basic\""}}, {{"weight", "4.5"}, {"title", "Heavy"}}};
    TEST_CHECK(game::writeTableText(dir + "/test_kinds.table", text, "TestKind"));
    std::string body;
    TEST_CHECK(readTextFile(dir + "/test_kinds.table", body));
    TEST_CHECK(writeTextFile(dir + "/test_kinds.table", body + "\nrow heavy\nweight 9\nrow bad id\nweight 2\n"));
    sim::Table<TestKind> kinds;
    TEST_CHECK(game::loadTable(kinds, "../build/does-not-exist.table") == false && kinds.size() == 0);
    game::TableData data;
    std::string warnings;
    TEST_CHECK(game::loadTableData(reflect::typeOf<TestKind>(), dir + "/test_kinds.table", data, &warnings));
    TEST_CHECK(data.ids.size() == 2 && data.ids[1] == "heavy" && !warnings.empty());
    TEST_CHECK(static_cast<const TestKind*>(data.rows[1].data())->weight == 4.5f);
    TEST_CHECK(static_cast<const TestKind*>(data.rows[0].data())->title == "Plain, \"basic\"");

    // CSV: 내보내고 가져오면 같다 (쉼표·따옴표가 든 칸)
    TEST_CHECK(game::exportTableCsv(data, dir + "/test_kinds.csv"));
    game::TableData back;
    back.rowType = &reflect::typeOf<TestKind>();
    std::string error;
    TEST_CHECK(game::importTableCsv(back, dir + "/test_kinds.csv", &error));
    TEST_CHECK(back.ids == data.ids);
    for (size_t i = 0; i < back.rows.size(); ++i)
        TEST_CHECK(reflect::hashOf(*static_cast<const TestKind*>(back.rows[i].data())) ==
                   reflect::hashOf(*static_cast<const TestKind*>(data.rows[i].data())));
    // 열 순서가 달라도, 모르는 열이 있어도 이름으로 맞춘다
    TEST_CHECK(writeTextFile(dir + "/test_kinds2.csv", "\xEF\xBB\xBFid,title,extra,weight\r\nx,\"multi\nline\",1,7\r\n"));
    TEST_CHECK(game::importTableCsv(back, dir + "/test_kinds2.csv", &error) && back.ids.size() == 1);
    TEST_CHECK(static_cast<const TestKind*>(back.rows[0].data())->weight == 7.0f);
    TEST_CHECK(static_cast<const TestKind*>(back.rows[0].data())->title == "multi\nline");
    TEST_CHECK(!game::importTableCsv(back, dir + "/test_kinds.table", &error)); // 첫 칸이 id가 아니다

    // 행 참조: id → 번호, 없는 id는 목록에
    sim::Table<TestKind> table;
    table.ids = {"plain", "heavy"};
    table.rows.resize(2);
    TestItem item;
    std::vector<std::string> missing;
    game::resolveRows(item, {game::tableIndex(table)}, &missing);
    TEST_CHECK(item.kind.index == 0 && missing.empty() && table.get(item.kind) == &table.rows[0]);
    TEST_CHECK(reflect::setFromText(reflect::typeOf<TestItem>(), &item, "kind", "heavy") && !item.kind.valid());
    game::resolveRows(item, {game::tableIndex(table)}, &missing);
    TEST_CHECK(item.kind.index == 1 && table.find("heavy") == 1 && table.find("nope") == kNoRow);
    item.kind.id = "nope";
    game::resolveRows(item, {game::tableIndex(table)}, &missing, "test: ");
    TEST_CHECK(!item.kind.valid() && missing.size() == 1 && missing[0].rfind("test: kind -> nope", 0) == 0);
    // 해시는 id로 (행 번호가 아니라)
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

TEST_CASE(strings_table) {
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
    TEST_CHECK(s.get("multi") == "한 줄\n두 줄"); // en에 없으면 기본 언어(ko)
    TEST_CHECK(s.get("no.such") == "no.such" && !s.has("no.such") && s.has("multi")); // 없으면 키 이름
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

TEST_CASE(ui_layout) {
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
    sys.layout({0, 0, 2000, 500}); // 보드 1000×500의 두 배 너비 → 배율 1, 보드가 가로로 늘어난다
    auto box = [&](const char* name) {
        for (const ui::NodeBox& b : sys.boxes(menu))
            if (b.name == name) return b;
        return ui::NodeBox{};
    };
    const ui::NodeBox bar = box("bar"), a = box("a"), grow = box("grow"), b = box("b");
    TEST_CHECK(bar.w == 2000 && bar.h == 100);                  // leftright: 양쪽에 붙어 늘어난다
    TEST_CHECK(a.x == 20 && a.y == 30 && a.w == 100);             // 여백 20, 가운데 정렬 (100 - 40) / 2
    TEST_CHECK(grow.x == 130 && grow.w == 2000 - 40 - 100 - 50 - 20 && grow.y == 40); // 채우기 = 남는 자리
    TEST_CHECK(b.x == 2000 - 20 - 50 && b.y == 20);
    TEST_CHECK(box("corner").x == 1900 && box("corner").y == 450); // 오른쪽 아래에 붙는다
    TEST_CHECK(box("middle").x == 950 && box("middle").y == 225);  // 가운데 기준
    u32 items = 0;
    for (const ui::NodeBox& n : sys.boxes(menu)) items += n.name == "item" ? 1u : 0u;
    TEST_CHECK(items == 3 && box("list").h == 3 * 30 + 2 * 5 && box("previewOnly").name.empty()); // 목록은 개수만큼, 예시 자식은 빠진다
    TEST_CHECK(!box("extra").visible);
    vm.showExtra = true;
    vm.rowsCount = 4;
    sys.layout({0, 0, 2000, 500});
    TEST_CHECK(box("extra").visible && box("list").h == 4 * 30 + 3 * 5);

    TEST_CHECK(sys.press(menu, "b") && vm.sound); // 토글은 자기 필드에 쓰고 액션을 낸다
    TEST_CHECK(sys.actions().size() == 1 && sys.actions()[0].name == "changed" && sys.actions()[0].field == "sound");
    TEST_CHECK(sys.info(menu).errors.empty() && sys.info(menu).nodes == 1 + 4 + 2 + 1 + 4 + 1);

    const ui::ScreenId bad = sys.open("ui/bad.ui");
    const ui::ScreenInfo info = sys.info(bad);
    TEST_CHECK(info.errors.size() == 2 && info.errors[0].find("(3)") != std::string::npos && info.errors[1].find("(4)") != std::string::npos);
    setProjectRoot(oldRoot);
}
#endif

MEALKIT_STRUCT(TestCommand) {
    MEALKIT_FIELD(u32, value, 0, "값");
    MEALKIT_FIELD(Vec3, where, {}, "위치");
};

MEALKIT_STRUCT(OtherCommand) {
    MEALKIT_FIELD(u8, flag, 0, "표시");
};

// 한 세션의 확정된 틱마다 명령을 모은다: "틱 플레이어 값" 줄들
struct CommandLog {
    std::vector<std::string> lines;
    void collect(u32 tick, const net::TickInputs& in) {
        sim::forEachCommand(in, [&](const sim::Command& c) {
            if (TestCommand t; c.read(t)) lines.push_back(std::to_string(tick) + " " + std::to_string(c.player) + " " + std::to_string(t.value));
            else if (c.is<OtherCommand>()) lines.push_back(std::to_string(tick) + " " + std::to_string(c.player) + " other");
        });
    }
};

TEST_CASE(command_channel) {
    { // 혼자: 보낸 명령은 다음에 확정되는 틱에, 보낸 순서로 들어간다
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

        // 틱당 크기를 넘으면 나머지는 다음 틱
        for (u32 i = 0; i < 40; ++i) {
            TestCommand c;
            c.value = 100 + i;
            sim::sendCommand(s, c);
        }
        CommandLog more; // 명령 하나 22바이트(머리 2 + 타입 4 + 16) → 틱당 11개
        for (u32 t = 2; t < 6; ++t) {
            s.submitLocalInput(t, {});
            more.collect(t, s.inputs(t));
            TEST_CHECK(s.inputs(t).commandBytes <= net::kCommandBytesPerTick);
        }
        TEST_CHECK(more.lines.size() == 40 && more.lines.front() == "2 0 100" && more.lines.back() == "5 0 139");
        TEST_CHECK(more.lines[10] == "2 0 110" && more.lines[11] == "3 0 111");

        // 입력 기록에 같이 들어간다
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
    { // 호스트 + 클라이언트 (UDP 루프백): 양쪽이 같은 틱에 같은 명령을 본다
        constexpr u16 kPort = 27931;
        net::LockstepSession host, client;
        TEST_CHECK(host.host(kPort, 2, 2, true));
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
                    if (t == 5 || t == 9) { // 클라이언트 2번, 호스트 1번
                        TestCommand c;
                        c.value = t * 10 + i;
                        if (i == 1 || t == 5) sim::sendCommand(s, c);
                    }
                    s.submitLocalInput(t + s.inputDelay(), {});
                }
            }
        }
        TEST_CHECK(next[0] == kTicks && next[1] == kTicks && !host.desynced());
        TEST_CHECK(logs[0].lines.size() == 3 && logs[0].lines == logs[1].lines);
        client.shutdown();
        host.shutdown();
    }
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

// 접촉 이벤트: 물리 스레드 수가 달라도 같은 목록 (정렬 순서), 해시도 같다
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

TEST_CASE(physics_contact_events) {
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
