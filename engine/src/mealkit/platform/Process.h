#pragma once

#include <string>
#include <vector>

namespace mealkit {

// Runs a command without a window and collects stdout/stderr (for the editor's game builds). Non-blocking.
class ChildProcess {
public:
    ChildProcess() = default;
    ~ChildProcess();
    ChildProcess(const ChildProcess&) = delete;
    ChildProcess& operator=(const ChildProcess&) = delete;

    // commandLine runs via cmd.exe /c (UTF-8). false if already running.
    bool start(const std::string& commandLine, const std::string& workingDirectory);
    // Appends newly produced output to the end of out (line based, converted to UTF-8). true if still running.
    bool poll(std::string& out);
    bool running() const { return m_process != nullptr; }
    int exitCode() const { return m_exitCode; }

private:
    void readAvailable(std::string& out, bool flush);
    void close();

    void* m_process = nullptr; // HANDLE
    void* m_pipe = nullptr;    // HANDLE (read end)
    std::string m_partial;     // bytes for which no newline has come yet
    int m_exitCode = 0;
};

// Launches several programs and closes them all at once (join windows of editor multiplayer play).
// They're put in a Job object, so they close too if the editor exits abnormally. Each process's stdout/stderr goes to the logPath file.
class ProcessGroup {
public:
    ProcessGroup() = default;
    ~ProcessGroup();
    ProcessGroup(const ProcessGroup&) = delete;
    ProcessGroup& operator=(const ProcessGroup&) = delete;

    bool launch(const std::string& exe, const std::string& arguments, const std::string& workingDirectory,
                const std::string& logPath);
    unsigned running() const; // number of processes still alive
    void terminateAll();

private:
    void* m_job = nullptr;          // HANDLE
    std::vector<void*> m_processes; // HANDLE
};

// Opens a folder in Explorer.
void openInExplorer(const std::string& path);

// Runs a program as a new process without waiting. arguments are appended to the command line as is (quoting is up to the caller).
bool launchDetached(const std::string& exe, const std::string& arguments, const std::string& workingDirectory);

} // namespace mealkit
