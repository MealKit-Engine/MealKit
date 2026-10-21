// 시스템 기능의 Windows 부분: 디버거, 오류 대화상자, 프로세스.
#include "mealkit/platform/System.h"

#include <windows.h>
#include <psapi.h>

#pragma comment(lib, "advapi32.lib") // GetUserNameW

namespace mealkit {

void debugOutput(const char* text) { OutputDebugStringA(text); }
bool debuggerAttached() { return IsDebuggerPresent() != FALSE; }
void debugBreak() { __debugbreak(); }

void showErrorDialog(const char* title, const char* message) {
    MessageBoxA(nullptr, message, title, MB_ICONERROR | MB_OK);
}

void exitProcess(int code) { ExitProcess(static_cast<UINT>(code)); }

u32 processId() { return static_cast<u32>(GetCurrentProcessId()); }

ProcessMemory processMemory() {
    PROCESS_MEMORY_COUNTERS c{};
    c.cb = sizeof(c);
    if (!K32GetProcessMemoryInfo(GetCurrentProcess(), &c, sizeof(c))) return {}; // kernel32 (psapi.lib 필요 없음)
    return {static_cast<u64>(c.WorkingSetSize), static_cast<u64>(c.PeakWorkingSetSize)};
}

std::string userName() {
    wchar_t name[256];
    DWORD size = 256;
    if (!GetUserNameW(name, &size) || size == 0) return {};
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, name, -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 1) return {};
    std::string out(static_cast<size_t>(bytes - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, name, -1, out.data(), bytes, nullptr, nullptr);
    return out;
}

} // namespace mealkit
