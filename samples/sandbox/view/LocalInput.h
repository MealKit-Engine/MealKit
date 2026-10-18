#pragma once

#include "sim/World.h"
#include "view/Camera.h"

#include "mealkit/input/Actions.h"

// 로컬 조작(입력 액션 move·jump·push, assets/input.bindings) → GameInput(8바이트).
// 이동 방향은 카메라 기준으로 계산해 월드 방향으로 양자화한다.
// 여기는 시뮬레이션 밖이라 삼각함수를 써도 된다 (결과 바이트만 전송된다).
namespace sandbox {

struct LocalInput {
    bool pushLatch = false;
    bool jumpLatch = false;

    // 매 프레임: 틱 사이에 짧게 누른 버튼을 다음 틱까지 기억한다
    void poll(const mealkit::input::Actions& actions);
    // 틱마다: 지금 조작을 입력으로 만든다. 쓴 뒤 clearLatches()
    GameInput sample(const mealkit::input::Actions& actions, const OrbitCamera& camera) const;
    void clearLatches() { pushLatch = jumpLatch = false; }
};

} // namespace sandbox
