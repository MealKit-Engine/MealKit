#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/render/DrawList.h"

#include <vector>

// 틱 이벤트(world.events)로 생기는 화면 효과. 지금은 밀치기·점프대 충격파 (소리·파티클이 생기면 같은 길로 받는다).
// 시간은 화면 시계(GameSession이 프레임 dt로 센다)라 시뮬레이션과 무관하다.
namespace sandbox {

struct Effects {
    struct Shockwave {
        Vec3 position;
        mealkit::f64 start = 0.0;
        f32 radius = 0.0f;
    };
    std::vector<Shockwave> shockwaves;

    void onEvent(const GameEvent& event, mealkit::f64 now, const ShockwaveLook& look);
    // 끝난 효과를 지운다 (그리지 않는 프레임에도 쌓이지 않게 매 프레임)
    void prune(mealkit::f64 now, const ShockwaveLook& look);
    void draw(mealkit::f64 now, const ShockwaveLook& look, mealkit::render::MeshId box,
              mealkit::render::DrawList& out) const;
};

} // namespace sandbox
