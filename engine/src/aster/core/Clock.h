#pragma once

#include "aster/core/Types.h"

namespace aster {

// 프로세스 시작 이후 경과 시간(초). QueryPerformanceCounter 기반, 단조 증가.
f64 nowSeconds();

// 프레임 상한. 노트북에서 배터리와 발열을 줄인다.
// 고해상도 대기 타이머로 자고, 마지막 1ms만 짧게 스핀한다.
class FrameLimiter {
public:
    FrameLimiter();
    ~FrameLimiter();
    FrameLimiter(const FrameLimiter&) = delete;
    FrameLimiter& operator=(const FrameLimiter&) = delete;

    void setTargetFps(f64 fps); // 0 = 제한 없음
    void wait();                // 프레임마다 한 번 호출

private:
    void* m_timer = nullptr;
    f64 m_interval = 0.0;
    f64 m_next = 0.0;
};

} // namespace aster
