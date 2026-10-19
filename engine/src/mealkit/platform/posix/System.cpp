// 시스템 기능의 POSIX 부분 (Linux·macOS 헤드리스). 대화상자는 없다(로그만).
#include "mealkit/platform/System.h"

#include <csignal>
#include <cstdlib>
#include <unistd.h>

namespace mealkit {

void debugOutput(const char*) {}
bool debuggerAttached() { return false; } // 필요해지면 /proc/self/status의 TracerPid (5단계)
void debugBreak() { std::raise(SIGTRAP); }
void showErrorDialog(const char*, const char*) {}
void exitProcess(int code) { std::_Exit(code); }
u32 processId() { return static_cast<u32>(getpid()); }

std::string userName() {
    const char* name = std::getenv("USER");
    return name ? std::string(name) : std::string();
}

} // namespace mealkit
