#pragma once

#include <string>

namespace mealkit::log {

enum class Level { Info, Warn, Error };

void write(Level level, const char* fmt, ...);

// Called for each log line (editor [Log] window). Can be called from any thread, so the receiver locks. nullptr = off
using Sink = void (*)(Level level, double seconds, const char* message);
void setSink(Sink sink);

// Recent log lines (up to kRecentLines, each "[seconds][level] text"): put into bug report bundles (game/BugReport.h)
inline constexpr unsigned kRecentLines = 2000;
std::string recentLines();

// Logs and terminates the process. Dialogs can be turned off with setDialogs(false) (for automated tests).
[[noreturn]] void fatal(const char* fmt, ...);
void setDialogs(bool enabled);
// MEALKIT_ASSERT failure: logs, and breaks right there if a debugger is attached.
void assertFailed(const char* condition, const char* file, int line);

} // namespace mealkit::log

#define MEALKIT_INFO(...) ::mealkit::log::write(::mealkit::log::Level::Info, __VA_ARGS__)
#define MEALKIT_WARN(...) ::mealkit::log::write(::mealkit::log::Level::Warn, __VA_ARGS__)
#define MEALKIT_ERROR(...) ::mealkit::log::write(::mealkit::log::Level::Error, __VA_ARGS__)
#define MEALKIT_FATAL(...) ::mealkit::log::fatal(__VA_ARGS__)

// Programmer error checks. On in Debug and Editor builds (MEALKIT_ASSERTS), removed from shipping builds.
#if defined(MEALKIT_DEBUG) || defined(MEALKIT_ASSERTS)
#define MEALKIT_ASSERT(cond)                                                                                           \
    do {                                                                                                               \
        if (!(cond)) ::mealkit::log::assertFailed(#cond, __FILE__, __LINE__);                                          \
    } while (0)
#else
#define MEALKIT_ASSERT(cond) ((void)0)
#endif
