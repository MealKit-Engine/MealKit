// Windows part of files/folders: executable location, opening UTF-8 paths, atomic replace-writes, file times, user folder.
#include "mealkit/platform/Paths.h"

#include <filesystem>
#include <share.h>
#include <windows.h>

namespace mealkit {
namespace fs = std::filesystem;

namespace {
std::string toUtf8(const fs::path& p) {
    const std::u8string s = p.u8string();
    return std::string(s.begin(), s.end());
}
fs::path fromUtf8(const std::string& s) { return fs::path(std::u8string(s.begin(), s.end())); }

std::wstring widen(const char* s) {
    const int len = MultiByteToWideChar(CP_UTF8, 0, s, -1, nullptr, 0);
    std::wstring out(len > 0 ? len - 1 : 0, L'\0');
    if (len > 1) MultiByteToWideChar(CP_UTF8, 0, s, -1, out.data(), len);
    return out;
}

std::string environmentVariable(const wchar_t* name) {
    wchar_t buffer[MAX_PATH];
    const DWORD len = GetEnvironmentVariableW(name, buffer, MAX_PATH);
    return len > 0 && len < MAX_PATH ? toUtf8(fs::path(std::wstring(buffer, len))) : std::string();
}

bool fileTimes(const std::string& path, unsigned long long* created, unsigned long long* written) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(fromUtf8(path).c_str(), GetFileExInfoStandard, &data)) return false;
    if (created)
        *created = (static_cast<unsigned long long>(data.ftCreationTime.dwHighDateTime) << 32) |
                   data.ftCreationTime.dwLowDateTime;
    if (written)
        *written = (static_cast<unsigned long long>(data.ftLastWriteTime.dwHighDateTime) << 32) |
                   data.ftLastWriteTime.dwLowDateTime;
    return true;
}
} // namespace

std::string executableDirectory() {
    wchar_t buffer[MAX_PATH];
    const DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return toUtf8(fs::path(std::wstring(buffer, len)).parent_path());
}

FILE* openFile(const std::string& path, const char* mode) {
    // Readable/writable even when opened elsewhere (like POSIX): bug reports read the input recording being written
    return _wfsopen(fromUtf8(path).c_str(), widen(mode).c_str(), _SH_DENYNO);
}

bool writeTextFile(const std::string& path, const std::string& text) {
    const fs::path target = fromUtf8(path);
    fs::path temp = target;
    temp += L".tmp";
    FILE* f = nullptr;
    if (_wfopen_s(&f, temp.c_str(), L"wb") != 0 || !f) return false;
    bool ok = fwrite(text.data(), 1, text.size(), f) == text.size();
    ok = fflush(f) == 0 && ok;
    fclose(f);
    if (!ok) {
        DeleteFileW(temp.c_str());
        return false;
    }
    return MoveFileExW(temp.c_str(), target.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != 0;
}

unsigned long long fileWriteTime(const std::string& path) {
    unsigned long long t = 0;
    return fileTimes(path, nullptr, &t) ? t : 0;
}

unsigned long long fileCreationTime(const std::string& path) {
    unsigned long long t = 0;
    return fileTimes(path, &t, nullptr) ? t : 0;
}

// FILETIME (100ns, since 1601) → local time string
std::string formatFileTime(unsigned long long time, bool withTime) {
    if (time == 0) return "-";
    FILETIME utc{static_cast<DWORD>(time), static_cast<DWORD>(time >> 32)}, local;
    SYSTEMTIME st;
    FileTimeToLocalFileTime(&utc, &local);
    FileTimeToSystemTime(&local, &st);
    char buf[32];
    if (withTime)
        std::snprintf(buf, sizeof(buf), "%04u-%02u-%02u %02u:%02u", st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute);
    else std::snprintf(buf, sizeof(buf), "%04u-%02u-%02u", st.wYear, st.wMonth, st.wDay);
    return buf;
}

std::string userDataDirectory() {
    const std::string local = environmentVariable(L"LOCALAPPDATA");
    return local.empty() ? executableDirectory() : local;
}

std::string documentsDirectory() {
    const std::string profile = environmentVariable(L"USERPROFILE");
    return profile.empty() ? std::string() : toUtf8(fromUtf8(profile) / "Documents");
}

} // namespace mealkit
