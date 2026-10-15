#include "mealkit/platform/Process.h"

#include "mealkit/core/Log.h"

#include <vector>
#include <windows.h>
#include <shellapi.h>

namespace mealkit {
namespace {

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

// 컴파일러·cmd 출력은 콘솔 코드 페이지(한국어 Windows는 CP949)일 수 있다. UTF-8이 아니면 OEM 코드 페이지로 본다.
std::string toUtf8Line(const std::string& bytes) {
    if (bytes.empty()) return {};
    if (MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0) >
        0)
        return bytes;
    const int n = MultiByteToWideChar(CP_OEMCP, 0, bytes.data(), static_cast<int>(bytes.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_OEMCP, 0, bytes.data(), static_cast<int>(bytes.size()), w.data(), n);
    const int m = WideCharToMultiByte(CP_UTF8, 0, w.data(), n, nullptr, 0, nullptr, nullptr);
    std::string out(static_cast<size_t>(m), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.data(), n, out.data(), m, nullptr, nullptr);
    return out;
}

} // namespace

ProcessGroup::~ProcessGroup() { terminateAll(); }

bool ProcessGroup::launch(const std::string& exe, const std::string& arguments, const std::string& workingDirectory,
                          const std::string& logPath) {
    if (!m_job) {
        m_job = CreateJobObjectW(nullptr, nullptr);
        if (!m_job) return false;
        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        SetInformationJobObject(m_job, JobObjectExtendedLimitInformation, &limits, sizeof(limits));
    }
    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE log = CreateFileW(widen(logPath).c_str(), GENERIC_WRITE, FILE_SHARE_READ, &sa, CREATE_ALWAYS,
                             FILE_ATTRIBUTE_NORMAL, nullptr);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    if (log != INVALID_HANDLE_VALUE) {
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
        si.hStdOutput = log;
        si.hStdError = log;
    }
    std::wstring cmd = L"\"" + widen(exe) + L"\" " + widen(arguments);
    const std::wstring dir = widen(workingDirectory);
    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
                                   nullptr, dir.empty() ? nullptr : dir.c_str(), &si, &pi);
    if (log != INVALID_HANDLE_VALUE) CloseHandle(log);
    if (!ok) {
        MEALKIT_ERROR("CreateProcess failed (%lu): %s %s", GetLastError(), exe.c_str(), arguments.c_str());
        return false;
    }
    AssignProcessToJobObject(m_job, pi.hProcess); // 시작하기 전에 넣어야 손자 프로세스까지 묶인다
    ResumeThread(pi.hThread);
    CloseHandle(pi.hThread);
    m_processes.push_back(pi.hProcess);
    return true;
}

unsigned ProcessGroup::running() const {
    unsigned count = 0;
    for (void* p : m_processes)
        if (WaitForSingleObject(p, 0) == WAIT_TIMEOUT) ++count;
    return count;
}

void ProcessGroup::terminateAll() {
    for (void* p : m_processes) {
        TerminateProcess(p, 0);
        CloseHandle(p);
    }
    m_processes.clear();
    if (m_job) {
        CloseHandle(m_job);
        m_job = nullptr;
    }
}

ChildProcess::~ChildProcess() { close(); }

bool ChildProcess::start(const std::string& commandLine, const std::string& workingDirectory) {
    if (running()) return false;
    m_partial.clear();
    m_exitCode = 0;

    SECURITY_ATTRIBUTES sa{sizeof(sa), nullptr, TRUE};
    HANDLE readPipe = nullptr, writePipe = nullptr;
    if (!CreatePipe(&readPipe, &writePipe, &sa, 0)) return false;
    SetHandleInformation(readPipe, HANDLE_FLAG_INHERIT, 0); // 읽기 쪽은 자식에게 넘기지 않는다

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = writePipe;
    si.hStdError = writePipe;

    // /s: 바깥 따옴표 한 쌍만 벗긴다 → 경로에 공백이 있어도 "C:\a b\build.bat" Export 형태가 그대로 전달된다
    std::wstring cmd = L"cmd.exe /s /c \"" + widen(commandLine) + L"\"";
    const std::wstring dir = widen(workingDirectory);
    PROCESS_INFORMATION pi{};
    const BOOL ok = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW, nullptr,
                                   dir.empty() ? nullptr : dir.c_str(), &si, &pi);
    CloseHandle(writePipe); // 자식이 끝나면 파이프가 닫히도록 부모 쪽 쓰기 핸들은 바로 닫는다
    if (!ok) {
        MEALKIT_ERROR("CreateProcess failed (%lu): %s", GetLastError(), commandLine.c_str());
        CloseHandle(readPipe);
        return false;
    }
    CloseHandle(pi.hThread);
    m_process = pi.hProcess;
    m_pipe = readPipe;
    return true;
}

void ChildProcess::readAvailable(std::string& out, bool flush) {
    HANDLE pipe = static_cast<HANDLE>(m_pipe);
    char buffer[4096];
    DWORD available = 0;
    while (PeekNamedPipe(pipe, nullptr, 0, nullptr, &available, nullptr) && available > 0) {
        DWORD read = 0;
        if (!ReadFile(pipe, buffer, sizeof(buffer), &read, nullptr) || read == 0) break;
        m_partial.append(buffer, read);
    }
    size_t start = 0;
    for (size_t nl = m_partial.find('\n'); nl != std::string::npos; nl = m_partial.find('\n', start)) {
        size_t end = nl;
        if (end > start && m_partial[end - 1] == '\r') --end;
        out += toUtf8Line(m_partial.substr(start, end - start));
        out += '\n';
        start = nl + 1;
    }
    m_partial.erase(0, start);
    if (flush && !m_partial.empty()) {
        out += toUtf8Line(m_partial);
        out += '\n';
        m_partial.clear();
    }
}

bool ChildProcess::poll(std::string& out) {
    if (!running()) return false;
    readAvailable(out, false);
    if (WaitForSingleObject(static_cast<HANDLE>(m_process), 0) != WAIT_OBJECT_0) return true;
    readAvailable(out, true); // 끝나기 직전에 쓴 출력
    DWORD code = 0;
    GetExitCodeProcess(static_cast<HANDLE>(m_process), &code);
    m_exitCode = static_cast<int>(code);
    close();
    return false;
}

// 실행 중이면 기다리지 않고 놓아준다 (빌드는 끝까지 진행된다)
void ChildProcess::close() {
    if (m_pipe) CloseHandle(static_cast<HANDLE>(m_pipe));
    if (m_process) CloseHandle(static_cast<HANDLE>(m_process));
    m_pipe = m_process = nullptr;
}

void openInExplorer(const std::string& path) {
    ShellExecuteW(nullptr, L"open", widen(path).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
}

bool launchDetached(const std::string& exe, const std::string& arguments, const std::string& workingDirectory) {
    std::wstring cmd = L"\"" + widen(exe) + L"\" " + widen(arguments);
    const std::wstring dir = widen(workingDirectory);
    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    if (!CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, 0, nullptr, dir.empty() ? nullptr : dir.c_str(),
                        &si, &pi)) {
        MEALKIT_ERROR("CreateProcess failed (%lu): %s", GetLastError(), exe.c_str());
        return false;
    }
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}

} // namespace mealkit
