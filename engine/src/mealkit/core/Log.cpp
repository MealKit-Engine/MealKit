#include "mealkit/core/Log.h"

#include "mealkit/core/Clock.h"

#include <cstdarg>
#include <cstdio>
#include <windows.h>

namespace mealkit::log {
namespace {

SRWLOCK g_lock = SRWLOCK_INIT;
bool g_dialogs = true;

void emit(Level level, const char* message) {
    const char* tag = level == Level::Info ? "INFO " : level == Level::Warn ? "WARN " : "ERROR";
    char line[2200];
    snprintf(line, sizeof(line), "[%8.3f][%s] %s\n", nowSeconds(), tag, message);

    AcquireSRWLockExclusive(&g_lock);
    FILE* out = level == Level::Info ? stdout : stderr;
    fputs(line, out);
    fflush(out);
    OutputDebugStringA(line);
    ReleaseSRWLockExclusive(&g_lock);
}

} // namespace

void write(Level level, const char* fmt, ...) {
    char message[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    emit(level, message);
}

void fatal(const char* fmt, ...) {
    char message[2048];
    va_list args;
    va_start(args, fmt);
    vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);
    emit(Level::Error, message);
    if (g_dialogs) {
        MessageBoxA(nullptr, message, "MealKit - fatal error", MB_ICONERROR | MB_OK);
    }
    ExitProcess(1);
}

void setDialogs(bool enabled) { g_dialogs = enabled; }

} // namespace mealkit::log
