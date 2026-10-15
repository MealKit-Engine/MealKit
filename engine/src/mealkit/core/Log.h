#pragma once

namespace mealkit::log {

enum class Level { Info, Warn, Error };

void write(Level level, const char* fmt, ...);

// 로그를 남기고 프로세스를 종료한다. 대화상자는 setDialogs(false)로 끌 수 있다(자동 테스트용).
[[noreturn]] void fatal(const char* fmt, ...);
void setDialogs(bool enabled);

} // namespace mealkit::log

#define MEALKIT_INFO(...) ::mealkit::log::write(::mealkit::log::Level::Info, __VA_ARGS__)
#define MEALKIT_WARN(...) ::mealkit::log::write(::mealkit::log::Level::Warn, __VA_ARGS__)
#define MEALKIT_ERROR(...) ::mealkit::log::write(::mealkit::log::Level::Error, __VA_ARGS__)
#define MEALKIT_FATAL(...) ::mealkit::log::fatal(__VA_ARGS__)

#if defined(MEALKIT_DEBUG)
#define MEALKIT_ASSERT(cond)                                                                   \
    do {                                                                                     \
        if (!(cond)) {                                                                       \
            MEALKIT_ERROR("Assertion failed: %s (%s:%d)", #cond, __FILE__, __LINE__);          \
            __debugbreak();                                                                  \
        }                                                                                    \
    } while (0)
#else
#define MEALKIT_ASSERT(cond) ((void)0)
#endif
