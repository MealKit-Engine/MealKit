#pragma once

namespace aster::log {

enum class Level { Info, Warn, Error };

void write(Level level, const char* fmt, ...);

// 로그를 남기고 프로세스를 종료한다. 대화상자는 setDialogs(false)로 끌 수 있다(자동 테스트용).
[[noreturn]] void fatal(const char* fmt, ...);
void setDialogs(bool enabled);

} // namespace aster::log

#define ASTER_INFO(...) ::aster::log::write(::aster::log::Level::Info, __VA_ARGS__)
#define ASTER_WARN(...) ::aster::log::write(::aster::log::Level::Warn, __VA_ARGS__)
#define ASTER_ERROR(...) ::aster::log::write(::aster::log::Level::Error, __VA_ARGS__)
#define ASTER_FATAL(...) ::aster::log::fatal(__VA_ARGS__)

#if defined(ASTER_DEBUG)
#define ASTER_ASSERT(cond)                                                                   \
    do {                                                                                     \
        if (!(cond)) {                                                                       \
            ASTER_ERROR("Assertion failed: %s (%s:%d)", #cond, __FILE__, __LINE__);          \
            __debugbreak();                                                                  \
        }                                                                                    \
    } while (0)
#else
#define ASTER_ASSERT(cond) ((void)0)
#endif
