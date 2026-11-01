#pragma once

#include "mealkit/core/Types.h"

#include <string>

namespace mealkit {

// CPU name (e.g. "Intel(R) Core(TM) i5-1135G7 @ 2.40GHz"). For recording determinism on other PCs. Empty string if unknown.
std::string cpuName();

// One line to the debugger output window (Windows OutputDebugString). Does nothing on platforms without one.
void debugOutput(const char* text);
bool debuggerAttached();
void debugBreak();

// Fatal error dialog (does nothing on windowless platforms/headless. The log was already written).
void showErrorDialog(const char* title, const char* message);
// Yes/no question dialog. false on platforms without dialogs.
bool askDialog(const char* title, const char* message);

// Crash handler (game/CrashReport.h): on an unhandled exception (Windows) or a fatal signal (POSIX), writes into folder
// crash.txt (exception code, address) and, on Windows, crash.dmp (minidump), then calls hook (folder exists, files only), then exits.
// One handler per process; calling again replaces it. Not installed while a debugger is attached (it should stop there instead).
using CrashHook = void (*)();
void installCrashHandler(const std::string& folder, CrashHook hook);

// If an exe built without a console window (Windows GUI program) was run from a command prompt, attaches stdout/stderr to that window (--help etc.).
// If output was redirected to a file/pipe, leaves it as is. Does nothing on other platforms.
void attachParentConsole();

// Exits immediately without cleanup (fatal error). Standard output is flushed first.
[[noreturn]] void exitProcess(int code);

u32 processId();
// Whether the process with that id is still alive (cleaning up live connection info files, mealkit/live/Server.h). true if unknown
bool processRunning(u32 pid);

// Real memory used by this process (bytes resident in RAM, Windows working set). peak is the largest since start. 0 if unknown.
// For benchmark reports/performance numbers (game::FrameStats).
struct ProcessMemory {
    u64 resident = 0;
    u64 peakResident = 0;
};
ProcessMemory processMemory();

// Environment variable (UTF-8). Empty string if missing
std::string environmentVariable(const char* name);

// This computer's user name (default player name). Empty string if unknown.
std::string userName();

} // namespace mealkit
