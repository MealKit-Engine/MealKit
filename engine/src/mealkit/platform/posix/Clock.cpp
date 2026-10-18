// 시간의 POSIX 구현 (Linux·macOS 헤드리스). clock_gettime(CLOCK_MONOTONIC) + nanosleep.
#include "mealkit/core/Clock.h"

#include <ctime>

namespace mealkit {
namespace {

f64 monotonic() {
    timespec ts{};
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return static_cast<f64>(ts.tv_sec) + static_cast<f64>(ts.tv_nsec) * 1e-9;
}

} // namespace

f64 nowSeconds() {
    static const f64 start = monotonic();
    return monotonic() - start;
}

void sleepSeconds(f64 seconds) {
    if (seconds <= 0.0) return;
    timespec ts{};
    ts.tv_sec = static_cast<time_t>(seconds);
    ts.tv_nsec = static_cast<long>((seconds - static_cast<f64>(ts.tv_sec)) * 1e9);
    nanosleep(&ts, nullptr);
}

FrameLimiter::FrameLimiter() = default;
FrameLimiter::~FrameLimiter() = default;

void FrameLimiter::setTargetFps(f64 fps) {
    m_interval = fps > 0.0 ? 1.0 / fps : 0.0;
    m_next = nowSeconds();
}

void FrameLimiter::wait() {
    if (m_interval <= 0.0) return;

    m_next += m_interval;
    const f64 now = nowSeconds();
    if (m_next < now - m_interval) m_next = now; // 크게 밀렸으면 따라잡지 말고 재동기화

    const f64 remaining = m_next - now;
    if (remaining > 0.002) {
        const f64 sleep = remaining - 0.001;
        timespec ts{};
        ts.tv_sec = static_cast<time_t>(sleep);
        ts.tv_nsec = static_cast<long>((sleep - static_cast<f64>(ts.tv_sec)) * 1e9);
        nanosleep(&ts, nullptr);
    }
    while (nowSeconds() < m_next) {}
}

} // namespace mealkit
