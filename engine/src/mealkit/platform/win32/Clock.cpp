// 시간의 Windows 구현: QueryPerformanceCounter, 고해상도 대기 타이머.
#include "mealkit/core/Clock.h"

#include <windows.h>

namespace mealkit {

f64 nowSeconds() {
    static const f64 invFrequency = [] {
        LARGE_INTEGER f;
        QueryPerformanceFrequency(&f);
        return 1.0 / static_cast<f64>(f.QuadPart);
    }();
    static const i64 start = [] {
        LARGE_INTEGER c;
        QueryPerformanceCounter(&c);
        return c.QuadPart;
    }();
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return static_cast<f64>(c.QuadPart - start) * invFrequency;
}

FrameLimiter::FrameLimiter() {
    m_timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!m_timer) {
        // Windows 10 1803 이전: 일반 타이머로 대체
        m_timer = CreateWaitableTimerExW(nullptr, nullptr, 0, TIMER_ALL_ACCESS);
    }
}

FrameLimiter::~FrameLimiter() {
    if (m_timer) CloseHandle(m_timer);
}

void FrameLimiter::setTargetFps(f64 fps) {
    m_interval = fps > 0.0 ? 1.0 / fps : 0.0;
    m_next = nowSeconds();
}

void FrameLimiter::wait() {
    if (m_interval <= 0.0) return;

    m_next += m_interval;
    f64 now = nowSeconds();
    if (m_next < now - m_interval) m_next = now; // 크게 밀렸으면 따라잡지 말고 재동기화

    const f64 remaining = m_next - now;
    if (remaining > 0.002 && m_timer) {
        LARGE_INTEGER due;
        due.QuadPart = -static_cast<LONGLONG>((remaining - 0.001) * 1e7); // 100ns 단위, 음수 = 상대 시간
        SetWaitableTimerEx(m_timer, &due, 0, nullptr, nullptr, nullptr, 0);
        WaitForSingleObject(m_timer, INFINITE);
    }
    while (nowSeconds() < m_next) YieldProcessor();
}

} // namespace mealkit
