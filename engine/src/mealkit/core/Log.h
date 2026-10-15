#pragma once

namespace mealkit::log {

enum class Level { Info, Warn, Error };

void write(Level level, const char* fmt, ...);

// 로그를 남기고 프로세스를 종료한다. 대화상자는 setDialogs(false)로 끌 수 있다(자동 테스트용).
[[noreturn]] void fatal(const char* fmt, ...);
void setDialogs(bool enabled);
// MEALKIT_ASSERT 실패: 로그를 남기고, 디버거가 붙어 있으면 그 자리에서 멈춘다.
void assertFailed(const char* condition, const char* file, int line);

} // namespace mealkit::log

#define MEALKIT_INFO(...) ::mealkit::log::write(::mealkit::log::Level::Info, __VA_ARGS__)
#define MEALKIT_WARN(...) ::mealkit::log::write(::mealkit::log::Level::Warn, __VA_ARGS__)
#define MEALKIT_ERROR(...) ::mealkit::log::write(::mealkit::log::Level::Error, __VA_ARGS__)
#define MEALKIT_FATAL(...) ::mealkit::log::fatal(__VA_ARGS__)

// 프로그래머 실수 검사. Debug와 Editor 빌드(MEALKIT_ASSERTS)에서 켜지고 출시 빌드에서는 빠진다.
#if defined(MEALKIT_DEBUG) || defined(MEALKIT_ASSERTS)
#define MEALKIT_ASSERT(cond)                                                      \
    do {                                                                        \
        if (!(cond)) ::mealkit::log::assertFailed(#cond, __FILE__, __LINE__);   \
    } while (0)
#else
#define MEALKIT_ASSERT(cond) ((void)0)
#endif
