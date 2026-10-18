// 파일·폴더 중 std::filesystem으로 되는 부분 (모든 플랫폼 공통). OS 호출은 win32/Paths.cpp, posix/Paths.cpp.
#include "mealkit/platform/Paths.h"

#include <algorithm>
#include <filesystem>
#include <system_error>

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
    FILE* f = openFile(path, "rb");
    if (!f) return false;
    out.clear();
    char buffer[16384];
    size_t n = 0;
    while ((n = fread(buffer, 1, sizeof(buffer), f)) > 0) out.append(buffer, n);
    const bool ok = !ferror(f);
    fclose(f);
    return ok;
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

std::vector<std::string> listFiles(const std::string& directory, const char* extension) {
    std::vector<std::string> out;
    std::error_code ec;
    for (const auto& entry : fs::directory_iterator(fromUtf8(directory), ec)) {
        if (!entry.is_regular_file(ec)) continue;
        if (extension && *extension && entry.path().extension() != fs::path(extension)) continue;
        out.push_back(toUtf8(entry.path().filename()));
    }
    std::sort(out.begin(), out.end());
    return out;
}

} // namespace mealkit
