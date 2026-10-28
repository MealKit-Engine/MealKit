#include "mealkit/core/Log.h"

#include "mealkit/core/Clock.h"
#include "mealkit/platform/System.h"

#include <cstdarg>
#include <cstdio>
#include <deque>
#include <mutex>

namespace mealkit::log {
namespace {

std::mutex g_lock;
bool g_dialogs = true;
Sink g_sink = nullptr;
std::deque<std::string> g_recent;

void emit(Level level, const char* message) {
    const char* tag = level == Level::Info ? "INFO " : level == Level::Warn ? "WARN " : "ERROR";
    const f64 seconds = nowSeconds();
    char line[2200];
    snprintf(line, sizeof(line), "[%8.3f][%s] %s\n", seconds, tag, message);

    std::lock_guard<std::mutex> guard(g_lock);
    FILE* out = level == Level::Info ? stdout : stderr;
    fputs(line, out);
    fflush(out);
    debugOutput(line);
    if (g_sink) g_sink(level, seconds, message);
    g_recent.emplace_back(line);
    if (g_recent.size() > kRecentLines) g_recent.pop_front();
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
    if (g_dialogs) showErrorDialog("MealKit - fatal error", message);
    exitProcess(1);
}

void setDialogs(bool enabled) { g_dialogs = enabled; }

void setSink(Sink sink) {
    std::lock_guard<std::mutex> guard(g_lock);
    g_sink = sink;
}

std::string recentLines() {
    std::lock_guard<std::mutex> guard(g_lock);
    std::string out;
    for (const std::string& l : g_recent) out += l;
    return out;
}

void assertFailed(const char* condition, const char* file, int line) {
    write(Level::Error, "Assertion failed: %s (%s:%d)", condition, file, line);
    if (debuggerAttached()) debugBreak();
}

} // namespace mealkit::log
