// Windows part of system features: debugger, error dialogs, processes.
#include "mealkit/platform/System.h"

#include <windows.h>
#include <psapi.h>
#include <dbghelp.h>

#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>

#pragma comment(lib, "advapi32.lib") // GetUserNameW

namespace mealkit {

void debugOutput(const char* text) { OutputDebugStringA(text); }
bool debuggerAttached() { return IsDebuggerPresent() != FALSE; }
void debugBreak() { __debugbreak(); }

void showErrorDialog(const char* title, const char* message) {
    MessageBoxA(nullptr, message, title, MB_ICONERROR | MB_OK);
}

namespace {

std::wstring widen(const char* s) {
    const int n = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    if (n <= 1) return {};
    std::wstring out(static_cast<size_t>(n - 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s, -1, out.data(), n);
    return out;
}

// MiniDumpWriteDump, loaded at install time (no dbghelp.lib): dbghelp.dll ships with Windows, and loading it inside the handler can fail
using MiniDumpWriteDumpFn = decltype(&MiniDumpWriteDump);

std::wstring g_crashFolder;
CrashHook g_crashHook = nullptr;
MiniDumpWriteDumpFn g_writeDump = nullptr;
constexpr DWORD kAbortException = 0xE0000AB0; // abort()/std::terminate/pure call → raised so the filter below sees it

LONG WINAPI onUnhandledException(EXCEPTION_POINTERS* info) {
    static volatile LONG entered = 0;
    if (InterlockedExchange(&entered, 1)) return EXCEPTION_EXECUTE_HANDLER; // a second thread crashing at the same time
    const EXCEPTION_RECORD* r = info ? info->ExceptionRecord : nullptr;
    char text[256];
    std::snprintf(text, sizeof(text), "exception = %08lx%s\naddress = %p\nthread = %lu\n", r ? r->ExceptionCode : 0ul,
                  r && r->ExceptionCode == kAbortException ? " (abort)" : "", r ? r->ExceptionAddress : nullptr,
                  GetCurrentThreadId());
    HANDLE f = CreateFileW((g_crashFolder + L"\\crash.txt").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
    if (f != INVALID_HANDLE_VALUE) {
        DWORD written = 0;
        WriteFile(f, text, static_cast<DWORD>(std::strlen(text)), &written, nullptr);
        CloseHandle(f);
    }
    if (g_writeDump) {
        HANDLE dump = CreateFileW((g_crashFolder + L"\\crash.dmp").c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, 0, nullptr);
        if (dump != INVALID_HANDLE_VALUE) {
            MINIDUMP_EXCEPTION_INFORMATION e{GetCurrentThreadId(), info, FALSE};
            // Small dump: stacks + the memory they point at
            g_writeDump(GetCurrentProcess(), GetCurrentProcessId(), dump, MiniDumpWithIndirectlyReferencedMemory,
                        info ? &e : nullptr, nullptr, nullptr);
            CloseHandle(dump);
        }
    }
    if (g_crashHook) g_crashHook();
    TerminateProcess(GetCurrentProcess(), r ? r->ExceptionCode : 1);
    return EXCEPTION_EXECUTE_HANDLER;
}

void raiseAbort() { RaiseException(kAbortException, EXCEPTION_NONCONTINUABLE, 0, nullptr); }
void onAbortSignal(int) { raiseAbort(); }
void onInvalidParameter(const wchar_t*, const wchar_t*, const wchar_t*, unsigned, uintptr_t) { raiseAbort(); }

} // namespace

bool askDialog(const char* title, const char* message) {
    return MessageBoxW(nullptr, widen(message).c_str(), widen(title).c_str(), MB_ICONWARNING | MB_YESNO) == IDYES;
}

void installCrashHandler(const std::string& folder, CrashHook hook) {
    if (IsDebuggerPresent()) return;
    g_crashFolder = widen(folder.c_str());
    g_crashHook = hook;
    if (!g_writeDump)
        if (HMODULE dbghelp = LoadLibraryW(L"dbghelp.dll"))
            g_writeDump = reinterpret_cast<MiniDumpWriteDumpFn>(GetProcAddress(dbghelp, "MiniDumpWriteDump"));
    SetUnhandledExceptionFilter(onUnhandledException);
    std::signal(SIGABRT, onAbortSignal);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT); // no CRT dialog before our handler
    _set_purecall_handler(raiseAbort);
    _set_invalid_parameter_handler(onInvalidParameter);
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
