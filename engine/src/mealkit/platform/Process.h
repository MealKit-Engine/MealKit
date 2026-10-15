#pragma once

#include <string>
#include <vector>

namespace mealkit {

// 창 없이 명령을 실행하고 stdout/stderr를 모은다 (에디터의 게임 빌드용). 블로킹하지 않는다.
class ChildProcess {
public:
    ChildProcess() = default;
    ~ChildProcess();
    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;

    // commandLine은 cmd.exe /c 로 실행된다 (UTF-8). 이미 실행 중이면 false.
    bool start(const std::string& commandLine, const std::string& workingDirectory);
    // 새로 나온 출력을 out 끝에 붙인다 (줄 단위, UTF-8로 변환). 아직 실행 중이면 true.
    bool poll(std::string& out);
    bool running() const { return m_process != nullptr; }
    int exitCode() const { return m_exitCode; }

private:
    void readAvailable(std::string& out, bool flush);
    void close();

    void* m_process = nullptr; // HANDLE
    void* m_pipe = nullptr;    // HANDLE (읽기 쪽)
    std::string m_partial;     // 아직 줄바꿈이 오지 않은 바이트
    int m_exitCode = 0;
};

// 프로그램 여러 개를 띄우고 한꺼번에 닫는다 (에디터 여러 명 플레이의 참가 창).
// Job 개체에 넣으므로 에디터가 비정상 종료해도 같이 닫힌다. 각 프로세스의 stdout/stderr는 logPath 파일로 간다.
class ProcessGroup {
public:
    ProcessGroup() = default;
    ~ProcessGroup();
    ProcessGroup(const ProcessGroup&) = delete;
    ProcessGroup& operator=(const ProcessGroup&) = delete;

    bool launch(const std::string& exe, const std::string& arguments, const std::string& workingDirectory,
                const std::string& logPath);
    unsigned running() const; // 아직 살아 있는 프로세스 수
    void terminateAll();

private:
    void* m_job = nullptr;           // HANDLE
    std::vector<void*> m_processes;  // HANDLE
};

// 탐색기로 폴더를 연다.
void openInExplorer(const std::string& path);

// 프로그램을 새 프로세스로 실행하고 기다리지 않는다. arguments는 그대로 명령줄에 붙는다 (따옴표는 호출하는 쪽에서).
bool launchDetached(const std::string& exe, const std::string& arguments, const std::string& workingDirectory);

} // namespace mealkit
