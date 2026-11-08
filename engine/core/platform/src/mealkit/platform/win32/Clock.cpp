// Windows implementation of time: QueryPerformanceCounter, high-resolution waitable timers.
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

void sleepSeconds(f64 seconds) {
    if (seconds <= 0.0) return;
    // One timer per thread (if creation fails, Sleep with about 15ms resolution)
    struct Timer {
        HANDLE handle = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
        ~Timer() {
            if (handle) CloseHandle(handle);
        }
    };
    thread_local Timer timer;
    if (!timer.handle) {
        Sleep(static_cast<DWORD>(seconds * 1000.0 + 0.5));
        return;
    }
    LARGE_INTEGER due;
    due.QuadPart = -static_cast<LONGLONG>(seconds * 1e7); // 100ns units, negative = relative time
    SetWaitableTimerEx(timer.handle, &due, 0, nullptr, nullptr, nullptr, 0);
    WaitForSingleObject(timer.handle, INFINITE);
}

FrameLimiter::FrameLimiter() {
    m_timer = CreateWaitableTimerExW(nullptr, nullptr, CREATE_WAITABLE_TIMER_HIGH_RESOLUTION, TIMER_ALL_ACCESS);
    if (!m_timer) {
        // Before Windows 10 1803: fall back to a normal timer
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
    if (m_next < now - m_interval) m_next = now; // if far behind, resync instead of catching up

    const f64 remaining = m_next - now;
    if (remaining > 0.002 && m_timer) {
        LARGE_INTEGER due;
        due.QuadPart = -static_cast<LONGLONG>((remaining - 0.001) * 1e7); // 100ns units, negative = relative time
        SetWaitableTimerEx(m_timer, &due, 0, nullptr, nullptr, nullptr, 0);
        WaitForSingleObject(m_timer, INFINITE);
    }
    while (nowSeconds() < m_next) YieldProcessor();
}

} // namespace mealkit
