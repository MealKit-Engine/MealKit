#pragma once

#include "mealkit/core/Types.h"

namespace mealkit {

// Seconds elapsed since process start. Based on QueryPerformanceCounter, monotonic.
f64 nowSeconds();

// Sleeps at least seconds (yields the CPU without busy-waiting). On Windows a high-resolution waitable timer wakes in 1ms steps.
// Used when there's nothing to do, like headless network waits. For precise frame intervals use FrameLimiter.
void sleepSeconds(f64 seconds);

// Frame cap. Reduces battery use and heat on laptops.
// Sleeps with a high-resolution waitable timer and spins briefly only for the last 1ms.
class FrameLimiter {
public:
    FrameLimiter();
    ~FrameLimiter();
    FrameLimiter(const FrameLimiter&) = delete;
    FrameLimiter& operator=(const FrameLimiter&) = delete;

    void setTargetFps(f64 fps); // 0 = unlimited
    void wait();                // call once per frame

private:
    void* m_timer = nullptr;
    f64 m_interval = 0.0;
    f64 m_next = 0.0;
};

} // namespace mealkit
