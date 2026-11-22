// Child processes on Linux/macOS: /bin/sh -c for command lines (cmd.exe /c on Windows), pipes for output.
#include "mealkit/platform/Process.h"

#include "mealkit/core/Log.h"

#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <unistd.h>
#if defined(__linux__)
#include <sys/prctl.h>
#endif

#include <cerrno>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>

namespace mealkit {
namespace {

// Child side after fork: working directory, then the shell. Only async-signal-safe calls until exec
[[noreturn]] void execShell(const std::string& command, const std::string& workingDirectory) {
    if (!workingDirectory.empty() && chdir(workingDirectory.c_str()) != 0) _exit(127);
    execl("/bin/sh", "sh", "-c", command.c_str(), static_cast<char*>(nullptr));
    _exit(127);
}

pid_t pidOf(void* p) { return static_cast<pid_t>(reinterpret_cast<std::intptr_t>(p)); }

} // namespace

ProcessGroup::~ProcessGroup() { terminateAll(); }

bool ProcessGroup::launch(const std::string& exe, const std::string& arguments, const std::string& workingDirectory,
                          const std::string& logPath) {
    const std::string command = "exec \"" + exe + "\" " + arguments;
    const pid_t parent = getpid();
    const pid_t pid = fork();
    if (pid < 0) {
        MEALKIT_ERROR("fork failed (%s): %s %s", std::strerror(errno), exe.c_str(), arguments.c_str());
        return false;
    }
    if (pid == 0) {
#if defined(__linux__)
        prctl(PR_SET_PDEATHSIG, SIGTERM); // like the Windows job object: closes if the editor exits abnormally
        if (getppid() != parent) _exit(0);
#endif
        setpgid(0, 0); // own process group, so terminateAll reaches its children too
        const int log = open(logPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
        if (log >= 0) {
            dup2(log, STDOUT_FILENO);
            dup2(log, STDERR_FILENO);
        }
        execShell(command, workingDirectory);
    }
    m_processes.push_back(reinterpret_cast<void*>(static_cast<std::intptr_t>(pid)));
    return true;
}

unsigned ProcessGroup::running() const {
    unsigned count = 0;
    for (void* p : m_processes) {
        int status = 0;
        const pid_t r = waitpid(pidOf(p), &status, WNOHANG);
        if (r == 0) ++count;
    }
    return count;
}

void ProcessGroup::terminateAll() {
    for (void* p : m_processes) {
        const pid_t pid = pidOf(p);
        kill(-pid, SIGTERM);
        kill(pid, SIGTERM);
        waitpid(pid, nullptr, WNOHANG);
    }
    m_processes.clear();
}

ChildProcess::~ChildProcess() { close(); }

bool ChildProcess::start(const std::string& commandLine, const std::string& workingDirectory) {
    if (running()) return false;
    m_partial.clear();
    m_exitCode = 0;
    int fds[2];
    if (pipe(fds) != 0) return false;
    const pid_t pid = fork();
    if (pid < 0) {
        MEALKIT_ERROR("fork failed (%s): %s", std::strerror(errno), commandLine.c_str());
        ::close(fds[0]);
        ::close(fds[1]);
        return false;
    }
    if (pid == 0) {
        ::close(fds[0]);
        dup2(fds[1], STDOUT_FILENO);
        dup2(fds[1], STDERR_FILENO);
        ::close(fds[1]);
        const int null = open("/dev/null", O_RDONLY);
        if (null >= 0) dup2(null, STDIN_FILENO);
        execShell(commandLine, workingDirectory);
    }
    ::close(fds[1]); // the pipe closes when the child ends
    fcntl(fds[0], F_SETFL, fcntl(fds[0], F_GETFL) | O_NONBLOCK);
    fcntl(fds[0], F_SETFD, FD_CLOEXEC);
    m_process = reinterpret_cast<void*>(static_cast<std::intptr_t>(pid));
    m_pipe = reinterpret_cast<void*>(static_cast<std::intptr_t>(fds[0] + 1)); // +1 so fd 0 isn't nullptr
    return true;
}

void ChildProcess::readAvailable(std::string& out, bool flush) {
    const int fd = static_cast<int>(reinterpret_cast<std::intptr_t>(m_pipe)) - 1;
    char buffer[4096];
    for (ssize_t n; (n = read(fd, buffer, sizeof(buffer))) > 0;) m_partial.append(buffer, static_cast<size_t>(n));
    size_t start = 0;
    for (size_t nl = m_partial.find('\n'); nl != std::string::npos; nl = m_partial.find('\n', start)) {
        size_t end = nl;
        if (end > start && m_partial[end - 1] == '\r') --end;
        out.append(m_partial, start, end - start);
        out += '\n';
        start = nl + 1;
    }
    m_partial.erase(0, start);
    if (flush && !m_partial.empty()) {
        out += m_partial;
        out += '\n';
        m_partial.clear();
    }
}

bool ChildProcess::poll(std::string& out) {
    if (!running()) return false;
    readAvailable(out, false);
    int status = 0;
    if (waitpid(pidOf(m_process), &status, WNOHANG) == 0) return true;
    readAvailable(out, true); // output written just before ending
    m_exitCode = WIFEXITED(status) ? WEXITSTATUS(status) : 128 + (WIFSIGNALED(status) ? WTERMSIG(status) : 0);
    m_process = nullptr; // reaped
    close();
    return false;
}

// If running, let it go without waiting (the build continues to completion; init reaps it once the editor exits)
void ChildProcess::close() {
    if (m_pipe) ::close(static_cast<int>(reinterpret_cast<std::intptr_t>(m_pipe)) - 1);
    m_pipe = m_process = nullptr;
}

void openInExplorer(const std::string& path) {
#if defined(__APPLE__)
    launchDetached("open", "\"" + path + "\"", "");
#else
    launchDetached("xdg-open", "\"" + path + "\"", "");
#endif
}

bool launchDetached(const std::string& exe, const std::string& arguments, const std::string& workingDirectory) {
    const std::string command = "exec \"" + exe + "\" " + arguments;
    const pid_t pid = fork();
    if (pid < 0) {
        MEALKIT_ERROR("fork failed (%s): %s", std::strerror(errno), exe.c_str());
        return false;
    }
    if (pid == 0) {
        // Double fork: the program is adopted by init, so no zombie is left and it outlives the editor
        setsid();
        if (fork() != 0) _exit(0);
        const int null = open("/dev/null", O_RDWR);
        if (null >= 0) {
            dup2(null, STDIN_FILENO);
        }
        execShell(command, workingDirectory);
    }
    int status = 0;
    waitpid(pid, &status, 0);
    return true;
}

bool hasCppCompiler() {
    if (const char* off = std::getenv("MEALKIT_NO_COMPILER"); off && off[0] == '1') return false;
    // tools/build.ps1 uses clang/clang++ (or $CXX), CMake and Ninja
    const char* cxx = std::getenv("CXX");
    const std::string compiler = cxx && *cxx ? cxx : "clang++";
    const std::string probe = "command -v \"" + compiler + "\" >/dev/null 2>&1 && command -v cmake >/dev/null 2>&1 && "
                              "command -v ninja >/dev/null 2>&1";
    return std::system(probe.c_str()) == 0;
}

} // namespace mealkit
