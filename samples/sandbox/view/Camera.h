#pragma once

#include "sim/World.h"
#include "view/ViewTuning.h"

#include "mealkit/platform/Window.h"
#include "mealkit/render/Renderer.h"

// 3인칭 궤도 카메라: 오른쪽 드래그로 회전, 휠로 줌. 시뮬레이션과 무관하므로 입력 지연 없이 바로 움직인다.
namespace sandbox {

// 값(시야각, 감도, 거리·각도 범위)은 화면용 튜닝(view/ViewTuning.h)의 CameraTuning.
struct OrbitCamera {
    f32 yaw = 0.0f;
    f32 pitch = 0.0f;
    f32 distance = 0.0f;
    bool orbiting = false;
    bool placed = false; // 처음 control()에서 튜닝의 시작 값으로 놓는다

    void control(mealkit::Window& window, bool mouseAllowed, const CameraTuning& tuning); // 매 프레임
    Vec3 forward() const; // 수평 앞 방향 (이동 입력을 월드 방향으로 바꿀 때)
    Vec3 eye(const Vec3& focus) const;
    // 뷰포트, 뷰·투영 행렬, 카메라 위치를 채운다
    void apply(const Vec3& focus, mealkit::render::Viewport viewport, const CameraTuning& tuning,
               mealkit::render::FrameParams& frame) const;
};

} // namespace sandbox
