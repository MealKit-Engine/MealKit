#include "mealkit/platform/Paths.h"

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

} // namespace mealkit
