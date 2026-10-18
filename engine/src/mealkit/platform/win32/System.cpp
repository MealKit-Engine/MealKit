// 시스템 기능의 Windows 부분: 디버거, 오류 대화상자, 프로세스.
#include "mealkit/platform/System.h"

#include <windows.h>

namespace mealkit {

void debugOutput(const char* text) { OutputDebugStringA(text); }
bool debuggerAttached() { return IsDebuggerPresent() != FALSE; }
void debugBreak() { __debugbreak(); }

void showErrorDialog(const char* title, const char* message) {
    MessageBoxA(nullptr, message, title, MB_ICONERROR | MB_OK);
}

void exitProcess(int code) { ExitProcess(static_cast<UINT>(code)); }

u32 processId() { return static_cast<u32>(GetCurrentProcessId()); }

} // namespace mealkit
