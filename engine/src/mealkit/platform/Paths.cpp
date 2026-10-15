#include "mealkit/platform/Paths.h"

#include <cstdio>
#include <filesystem>
#include <system_error>
#include <windows.h>

namespace mealkit {
namespace fs = std::filesystem;

namespace {
std::string toUtf8(const fs::path& p) {
    const std::u8string s = p.u8string();
    return std::string(s.begin(), s.end());
}
fs::path fromUtf8(const std::string& s) { return fs::path(std::u8string(s.begin(), s.end())); }

std::string& projectRootStorage() {
    static std::string root;
    return root;
}
} // namespace

void setProjectRoot(const std::string& directory) { projectRootStorage() = directory; }
const std::string& projectRoot() { return projectRootStorage(); }

std::string executableDirectory() {
    wchar_t buffer[MAX_PATH];
    const DWORD len = GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    return toUtf8(fs::path(std::wstring(buffer, len)).parent_path());
}

std::string findAsset(const std::string& relative) {
    std::error_code ec;
    const fs::path rel = fromUtf8(relative);
    if (!projectRoot().empty()) {
        const fs::path candidate = fromUtf8(projectRoot()) / rel;
        return fs::exists(candidate, ec) ? toUtf8(candidate) : std::string();
    }
    fs::path dir = fromUtf8(executableDirectory());
    for (int depth = 0; depth < 6 && !dir.empty(); ++depth) {
        const fs::path candidate = dir / rel;
        if (fs::exists(candidate, ec)) return toUtf8(candidate);
        const fs::path parent = dir.parent_path();
        if (parent == dir) break;
        dir = parent;
    }
    const fs::path cwd = fs::current_path(ec) / rel;
    if (!ec && fs::exists(cwd, ec)) return toUtf8(cwd);
    return {};
}

bool fileExists(const std::string& path) {
    std::error_code ec;
    return fs::is_regular_file(fromUtf8(path), ec);
}

bool directoryExists(const std::string& path) {
    std::error_code ec;
    return fs::is_directory(fromUtf8(path), ec);
}

std::string fileName(const std::string& path) { return toUtf8(fromUtf8(path).filename()); }

std::string parentDirectory(const std::string& path) { return toUtf8(fromUtf8(path).parent_path()); }

bool readTextFile(const std::string& path, std::string& out) {
    FILE* f = nullptr;
    if (_wfopen_s(&f, fromUtf8(path).c_str(), L"rb") != 0 || !f) return false;
    out.clear();
    char buffer[16384];
    size_t n = 0;
    while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) out.append(buffer, n);
    const bool ok = !ferror(f);
    fclose(f);
    return ok;
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

bool createDirectories(const std::string& path) {
    std::error_code ec;
    fs::create_directories(fromUtf8(path), ec);
    return directoryExists(path);
}

bool removeFile(const std::string& path) {
    std::error_code ec;
    return fs::remove(fromUtf8(path), ec);
}

unsigned long long fileWriteTime(const std::string& path) {
    WIN32_FILE_ATTRIBUTE_DATA data{};
    if (!GetFileAttributesExW(fromUtf8(path).c_str(), GetFileExInfoStandard, &data)) return 0;
    return (static_cast<unsigned long long>(data.ftLastWriteTime.dwHighDateTime) << 32) | data.ftLastWriteTime.dwLowDateTime;
}

} // namespace mealkit
