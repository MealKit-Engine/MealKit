// Windows part of system features: debugger, error dialogs, processes.
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
    if (GetConsoleWindow()) return; // a console program or already attached
    const bool outRedirected = GetStdHandle(STD_OUTPUT_HANDLE) != nullptr;
    const bool errRedirected = GetStdHandle(STD_ERROR_HANDLE) != nullptr;
    if (outRedirected && errRedirected) return; // being captured by a file/pipe (check scripts)
    if (!AttachConsole(ATTACH_PARENT_PROCESS)) return; // run from Explorer: no console
    FILE* f = nullptr;
    if (!outRedirected) freopen_s(&f, "CONOUT$", "w", stdout);
    if (!errRedirected) freopen_s(&f, "CONOUT$", "w", stderr);
}

void exitProcess(int code) { ExitProcess(static_cast<UINT>(code)); }

u32 processId() { return static_cast<u32>(GetCurrentProcessId()); }

bool processRunning(u32 pid) {
    HANDLE h = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
    if (!h) return GetLastError() == ERROR_ACCESS_DENIED; // another user's process: alive
    DWORD code = 0;
    const bool alive = GetExitCodeProcess(h, &code) && code == STILL_ACTIVE;
    CloseHandle(h);
    return alive;
}

ProcessMemory processMemory() {
    PROCESS_MEMORY_COUNTERS c{};
    c.cb = sizeof(c);
    if (!K32GetProcessMemoryInfo(GetCurrentProcess(), &c, sizeof(c))) return {}; // kernel32 (no psapi.lib needed)
    return {static_cast<u64>(c.WorkingSetSize), static_cast<u64>(c.PeakWorkingSetSize)};
}

std::string environmentVariable(const char* name) {
    std::wstring wname(name, name + std::strlen(name)); // the name is ASCII
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
