#pragma once

#include "mealkit/core/Types.h"

#include <string>

namespace mealkit {

// CPU 이름 (예: "Intel(R) Core(TM) i5-1135G7 @ 2.40GHz"). 다른 PC 결정론 기록용. 모르면 빈 문자열.
std::string cpuName();

// 디버거 출력 창에 한 줄 (Windows OutputDebugString). 없는 플랫폼에서는 아무것도 하지 않는다.
void debugOutput(const char* text);
bool debuggerAttached();
void debugBreak();

// 치명적 오류 대화상자 (창이 없는 플랫폼·헤드리스에서는 아무것도 하지 않는다. 로그는 이미 남았다).
void showErrorDialog(const char* title, const char* message);

// 정리 없이 바로 끝낸다 (치명적 오류). 표준 출력은 미리 비워 둔다.
[[noreturn]] void exitProcess(int code);

u32 processId();
// 그 번호의 프로세스가 아직 살아 있나 (라이브 연결 접속 정보 파일 정리, mealkit/live/Server.h). 모르면 true
bool processRunning(u32 pid);

// 이 프로세스가 쓰는 실제 메모리 (RAM에 올라 있는 바이트, Windows 작업 집합). peak는 시작부터 가장 컸던 값. 모르면 0.
// 벤치마크 보고서·성능 숫자용 (game::FrameStats).
struct ProcessMemory {
    u64 resident = 0;
    u64 peakResident = 0;
};
ProcessMemory processMemory();

// 환경 변수 (UTF-8). 없으면 빈 문자열
std::string environmentVariable(const char* name);

// 이 컴퓨터의 사용자 이름 (플레이어 이름의 기본값). 모르면 빈 문자열.
std::string userName();

} // namespace mealkit
