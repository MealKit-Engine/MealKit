// 시스템 기능의 POSIX 부분 (Linux·macOS 헤드리스). 대화상자는 없다(로그만).
#include "mealkit/platform/System.h"

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <sys/resource.h>
#include <unistd.h>
#if defined(__APPLE__)
#include <mach/mach.h>
#endif

namespace mealkit {

void debugOutput(const char*) {}
bool debuggerAttached() { return false; } // 필요해지면 /proc/self/status의 TracerPid (5단계)
void debugBreak() { std::raise(SIGTRAP); }
void showErrorDialog(const char*, const char*) {}
void exitProcess(int code) { std::_Exit(code); }
u32 processId() { return static_cast<u32>(getpid()); }

bool processRunning(u32 pid) { return kill(static_cast<pid_t>(pid), 0) == 0 || errno == EPERM; }

ProcessMemory processMemory() {
    ProcessMemory out;
    rusage usage{};
#if defined(__APPLE__)
    mach_task_basic_info info{};
    mach_msg_type_number_t count = MACH_TASK_BASIC_INFO_COUNT;
    if (task_info(mach_task_self(), MACH_TASK_BASIC_INFO, reinterpret_cast<task_info_t>(&info), &count) == KERN_SUCCESS)
        out.resident = info.resident_size;
    if (getrusage(RUSAGE_SELF, &usage) == 0) out.peakResident = static_cast<u64>(usage.ru_maxrss); // macOS: 바이트
#else
    if (FILE* f = std::fopen("/proc/self/statm", "r")) { // 페이지 단위: 전체 크기, 상주
        unsigned long long size = 0, resident = 0;
        if (std::fscanf(f, "%llu %llu", &size, &resident) == 2)
            out.resident = resident * static_cast<u64>(sysconf(_SC_PAGESIZE));
        std::fclose(f);
    }
    if (getrusage(RUSAGE_SELF, &usage) == 0) out.peakResident = static_cast<u64>(usage.ru_maxrss) * 1024; // Linux: KB
#endif
    return out;
}

std::string environmentVariable(const char* name) {
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
}

std::string userName() {
    const char* name = std::getenv("USER");
    return name ? std::string(name) : std::string();
}

} // namespace mealkit
