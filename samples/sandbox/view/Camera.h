#pragma once

#include "sim/World.h"

#include "mealkit/platform/Window.h"
#include "mealkit/render/Renderer.h"

// 3인칭 궤도 카메라: 오른쪽 드래그로 회전, 휠로 줌. 시뮬레이션과 무관하므로 입력 지연 없이 바로 움직인다.
namespace sandbox {

struct OrbitCamera {
    f32 yaw = 0.8f;
    f32 pitch = 0.55f;
    f32 distance = 18.0f;
    bool orbiting = false;

    void control(mealkit::Window& window, bool mouseAllowed); // 매 프레임
    Vec3 forward() const;                                     // 수평 앞 방향 (이동 입력을 월드 방향으로 바꿀 때)
    Vec3 eye(const Vec3& focus) const;
    // 뷰포트, 뷰·투영 행렬, 카메라 위치를 채운다
    void apply(const Vec3& focus, mealkit::render::Viewport viewport, mealkit::render::FrameParams& frame) const;
};

} // namespace sandbox
