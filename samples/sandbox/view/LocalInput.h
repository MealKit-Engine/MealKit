#pragma once

#include "sim/World.h"
#include "view/Camera.h"

#include "mealkit/platform/Input.h"

// 로컬 조작 → GameInput(8바이트). 이동 방향은 카메라 기준으로 계산해 월드 방향으로 양자화한다.
// 여기는 시뮬레이션 밖이라 삼각함수를 써도 된다 (결과 바이트만 전송된다).
namespace sandbox {

struct LocalInput {
    bool pushLatch = false;
    bool jumpLatch = false;

    // 매 프레임: 틱 사이에 짧게 누른 버튼을 다음 틱까지 기억한다
    void poll(mealkit::Input& input, bool keyboardAllowed, bool mouseAllowed);
    // 틱마다: 지금 조작을 입력으로 만든다. 쓴 뒤 clearLatches()
    GameInput sample(mealkit::Input& input, const OrbitCamera& camera, bool keyboardAllowed, bool mouseAllowed) const;
    void clearLatches() { pushLatch = jumpLatch = false; }
};

} // namespace sandbox
