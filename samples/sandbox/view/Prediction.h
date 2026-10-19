#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/net/Lockstep.h"

// 내 캐릭터 겉모습만 미리 움직이기 (ROADMAP 1단계 "내 캐릭터 겉모습만 예측"): 입력 지연(수십~수백 ms) 동안
// 보냈지만 아직 실행되지 않은 내 이동 입력만큼 내 캐릭터를 앞서 그린다. 시뮬레이션은 그대로이고(결정론과 무관),
// 확정 상태가 따라오면 차이가 줄어 부드럽게 맞춰진다. 수평 이동만 미리 보이고, 점프·밀치기·다른 캐릭터와의 충돌은 확정 뒤에 보인다.
namespace sandbox {

struct LocalPrediction {
    Vec3 offset; // 지금 그리는 차이 (확정 위치 기준)

    // 매 프레임: 이번에 더할 이동(아직 실행 안 된 입력들의 합)을 받고 offset을 그쪽으로 부드럽게 옮긴다
    void begin() { m_target = {}; }
    void addInput(const World& world, u8 player, const mealkit::net::PlayerInput& input);
    void update(f32 dt, const PredictionLook& look, bool active);
    void reset() { offset = m_target = {}; }

private:
    Vec3 m_target;
};

} // namespace sandbox
