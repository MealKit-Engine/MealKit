// 시스템 기능의 Windows 부분: 디버거, 오류 대화상자, 프로세스.
#include "mealkit/platform/System.h"

#include <windows.h>
#include <psapi.h>

#include <cstdio>
#include <cstring>

#pragma comment(lib, "advapi32.lib") // GetUserNameW

namespace mealkit {

void debugOutput(const char* text) { OutputDebugStringA(text); }
bool debuggerAttached() { return IsDebuggerPresent() != FALSE; }
void debugBreak() { __debugbreak(); }

void showErrorDialog(const char* title, const char* message) {
    MessageBoxA(nullptr, message, title, MB_ICONERROR | MB_OK);
}

void attachParentConsole() {
    if (GetConsoleWindow()) return; // 콘솔 프로그램이거나 이미 붙었다
    const bool outRedirected = GetStdHandle(STD_OUTPUT_HANDLE) != nullptr;
    const bool errRedirected = GetStdHandle(STD_ERROR_HANDLE) != nullptr;
    if (outRedirected && errRedirected) return; // 파일·파이프로 받고 있다 (검사 스크립트)
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) return; // 탐색기에서 실행: 콘솔 없음
    FILE* f = nullptr;
    if (!outRedirected) freopen_s(&f, "CONOUT$", "w", stdout);
    if (!errRedirected) freopen_s(&f, "CONOUT$", "w", stderr);
}

void exitProcess(int code) { ExitProcess(static_cast<UINT>(code)); }

u32 processId() { return static_cast<u32>(GetCurrentProcessId()); }

bool processRunning(u32 pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return GetLastError() == ERROR_ACCESS_DENIED; // 다른 사용자의 프로세스: 살아 있다
    DWORD code = 0;
    const bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return alive;
}

ProcessMemory processMemory() {
    PROCESS_MEMORY_COUNTERS c{};
    c.cb = sizeof(c);
    if (!K32GetProcessMemoryInfo(GetCurrentProcess(), &c, sizeof(c))) return {}; // kernel32 (psapi.lib 필요 없음)
    return {static_cast<u64>(c.WorkingSetSize), static_cast<u64>(c.PeakWorkingSetSize)};
}

std::string environmentVariable(const char* name) {
    std::wstring wname(name, name + std::strlen(name)); // 이름은 ASCII
    const DWORD size = GetEnvironmentVariableW(wname.c_str(), nullptr, 0);
    if (size == 0) return {};
    std::wstring value(size, L'\0');
    const DWORD got = GetEnvironmentVariableW(wname.c_str(), value.data(), size);
    value.resize(got);
    const int bytes = WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 1) return {};
    std::string out(static_cast<size_t>(bytes - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, value.c_str(), -1, out.data(), bytes, nullptr, nullptr);
    return out;
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
