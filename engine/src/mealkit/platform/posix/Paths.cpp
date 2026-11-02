// POSIX part of files/folders (Linux/macOS headless): executable location, atomic replace-writes, file times, user folder.
#include "mealkit/platform/Paths.h"

#include "mealkit/platform/Pack.h"

#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <sys/stat.h>
#include <unistd.h>

#if defined(__APPLE__)
#include <mach-o/dyld.h>
#endif

namespace mealkit {
namespace fs = std::filesystem;

namespace {
std::string environmentVariable(const char* name) {
    const char* value = std::getenv(name);
    return value ? std::string(value) : std::string();
}
} // namespace

std::string executableDirectory() {
#if defined(__APPLE__)
    char buffer[4096];
    uint32_t size = sizeof(buffer);
    if (_NSGetExecutablePath(buffer, &size) != 0) return ".";
    std::error_code ec;
    const fs::path exe = fs::weakly_canonical(fs::path(buffer), ec);
#else
    std::error_code ec;
    const fs::path exe = fs::read_symlink("/proc/self/exe", ec);
#endif
    return ec ? std::string(".") : exe.parent_path().string();
}

FILE* openFile(const std::string& path, const char* mode) {
    if (FILE* packed = pack::open(path, mode)) return packed;
    return std::fopen(path.c_str(), mode);
}

bool writeTextFile(const std::string& path, const std::string& text) {
    const std::string temp = path + ".tmp";
    FILE* f = std::fopen(temp.c_str(), "wb");
    if (!f) return false;
    bool ok = fwrite(text.data(), 1, text.size(), f) == text.size();
    ok = fflush(f) == 0 && ok;
    ok = fsync(fileno(f)) == 0 && ok;
    fclose(f);
    if (!ok) {
        std::remove(temp.c_str());
        return false;
    }
    return std::rename(temp.c_str(), path.c_str()) == 0; // atomic within the same file system
}

// Times are Unix time in nanoseconds
unsigned long long fileWriteTime(const std::string& path) {
    if (const unsigned long long t = pack::writeTime(path)) return t;
    struct stat st{};
    if (stat(path.c_str(), &st) != 0) return 0;
#if defined(__APPLE__)
    return static_cast<unsigned long long>(st.st_mtimespec.tv_sec) * 1000000000ull + st.st_mtimespec.tv_nsec;
#else
    return static_cast<unsigned long long>(st.st_mtim.tv_sec) * 1000000000ull + st.st_mtim.tv_nsec;
#endif
}

unsigned long long fileCreationTime(const std::string& path) {
    if (const unsigned long long t = pack::writeTime(path)) return t;
#if defined(__APPLE__)
    struct stat st{};
    if (stat(path.c_str(), &st) != 0) return 0;
    return static_cast<unsigned long long>(st.st_birthtimespec.tv_sec) * 1000000000ull + st.st_birthtimespec.tv_nsec;
#else
    return fileWriteTime(path); // Linux stat has no creation time (statx with the stage 5 editor port)
#endif
}

std::string formatFileTime(unsigned long long time, bool withTime) {
    if (time == 0) return "-";
    const time_t seconds = static_cast<time_t>(time / 1000000000ull);
    tm local{};
    localtime_r(&seconds, &local);
    char buf[32];
    std::strftime(buf, sizeof(buf), withTime ? "%Y-%m-%d %H:%M" : "%Y-%m-%d", &local);
    return buf;
}

std::string userDataDirectory() {
    const std::string home = environmentVariable("HOME");
#if defined(__APPLE__)
    if (!home.empty()) return home + "/Library/Application Support";
#else
    const std::string xdg = environmentVariable("XDG_DATA_HOME");
    if (!xdg.empty()) return xdg;
    if (!home.empty()) return home + "/.local/share";
#endif
    return executableDirectory();
}

std::string documentsDirectory() {
    const std::string home = environmentVariable("HOME");
    return home.empty() ? std::string() : home + "/Documents";
}

} // namespace mealkit
