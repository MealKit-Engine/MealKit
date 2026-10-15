// 엔진 단위 테스트 (mealkit_tests.exe). 엔진 저장소 빌드에만 있다. tools\check.ps1이 Release와 Editor 빌드에서 돌린다.
//   mealkit_tests.exe          모든 테스트, 끝에 PASS 또는 FAILED
//   출력의 "detmath-bits <hash>" 줄은 결정론 수학 결과 비트의 해시다. 빌드 구성이 달라도 같아야 한다.
#include "mealkit/core/DetMath.h"
#include "mealkit/core/Hash.h"
#include "mealkit/core/Log.h"
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
    TEST_CHECK(countIter(p) == 0);                 // 만든 틱에는 순회에 안 나온다
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

} // namespace

int main() {
    log::setDialogs(false);
    return test::runAll();
}
