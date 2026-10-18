#pragma once

#include "mealkit/core/Types.h"

namespace mealkit {

// 프로세스 시작 이후 경과 시간(초). QueryPerformanceCounter 기반, 단조 증가.
f64 nowSeconds();

// 적어도 seconds만큼 잔다 (바쁜 대기 없이 CPU를 내준다). Windows는 고해상도 대기 타이머라 1ms 단위로 깬다.
// 헤드리스 네트워크 대기처럼 할 일이 없을 때 쓴다. 정확한 프레임 간격은 FrameLimiter.
void sleepSeconds(f64 seconds);

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

} // namespace mealkit
