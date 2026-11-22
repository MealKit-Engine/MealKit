// Linux file dialogs: zenity (GNOME, installed with Ubuntu desktops) or kdialog (KDE), run as a child process.
// The dialog is modal to the user only: the editor waits for it on the main thread like the Windows common dialogs.
#include "mealkit/platform/FileDialog.h"

#include "mealkit/core/Log.h"

#include <spawn.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cstdlib>
#include <string>
#include <vector>

extern char** environ;

namespace mealkit {
namespace {

bool onPath(const char* program) {
    const char* path = std::getenv("PATH");
    if (!path) return false;
    std::string dirs = path;
    size_t start = 0;
    while (start <= dirs.size()) {
        const size_t end = dirs.find(':', start);
        const std::string dir = dirs.substr(start, end == std::string::npos ? std::string::npos : end - start);
        if (!dir.empty() && access((dir + "/" + program).c_str(), X_OK) == 0) return true;
        if (end == std::string::npos) break;
        start = end + 1;
    }
    return false;
}

// Runs argv and returns its stdout without the trailing newline (empty on cancel or failure)
std::string run(const std::vector<std::string>& args) {
    int pipeFds[2];
    if (pipe(pipeFds) != 0) return {};
    posix_spawn_file_actions_t actions;
    posix_spawn_file_actions_init(&actions);
    posix_spawn_file_actions_adddup2(&actions, pipeFds[1], STDOUT_FILENO);
    posix_spawn_file_actions_addclose(&actions, pipeFds[0]);
    posix_spawn_file_actions_addclose(&actions, pipeFds[1]);
    std::vector<char*> argv;
    for (const std::string& a : args) argv.push_back(const_cast<char*>(a.c_str()));
    argv.push_back(nullptr);
    pid_t pid = 0;
    const int error = posix_spawnp(&pid, argv[0], &actions, nullptr, argv.data(), environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipeFds[1]);
    std::string out;
    if (error == 0) {
        char buffer[1024];
        for (ssize_t n; (n = read(pipeFds[0], buffer, sizeof(buffer))) > 0;) out.append(buffer, static_cast<size_t>(n));
        int status = 0;
        waitpid(pid, &status, 0);
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) out.clear();
    }
    close(pipeFds[0]);
    while (!out.empty() && (out.back() == '\n' || out.back() == '\r')) out.pop_back();
    return out;
}

// "*.glb;*.gltf" → "*.glb *.gltf"
std::string spaced(const char* patterns) {
    std::string s = patterns;
    for (char& c : s)
        if (c == ';') c = ' ';
    return s;
}

std::string startPath(const std::string& initialPath, const std::string& initialFolder) {
    if (!initialPath.empty()) return initialPath;
    if (!initialFolder.empty()) return initialFolder.back() == '/' ? initialFolder : initialFolder + "/";
    return {};
}

enum class Kind { Open, Save, Folder };

std::string dialog(Kind kind, const std::string& initialPath, const std::string& initialFolder, const FileFilter* filters,
                   int count, const char* defaultExtension) {
    std::vector<std::string> args;
    const std::string start = startPath(initialPath, initialFolder);
    if (onPath("zenity")) {
        args = {"zenity", "--file-selection"};
        if (kind == Kind::Save) args.insert(args.end(), {"--save", "--confirm-overwrite"});
        if (kind == Kind::Folder) args.push_back("--directory");
        if (!start.empty()) args.push_back("--filename=" + start);
        for (int i = 0; i < count; ++i)
            args.push_back(std::string("--file-filter=") + filters[i].name + " | " + spaced(filters[i].patterns));
    } else if (onPath("kdialog")) {
        args = {"kdialog", kind == Kind::Open ? "--getopenfilename" : kind == Kind::Save ? "--getsavefilename" : "--getexistingdirectory",
                start.empty() ? "." : start};
        if (kind != Kind::Folder && count > 0) {
            std::string filter;
            for (int i = 0; i < count; ++i)
                filter += (i ? "\n" : "") + spaced(filters[i].patterns) + "|" + filters[i].name;
            args.push_back(filter);
        }
    } else {
        MEALKIT_ERROR("No file dialog program found: install zenity (GNOME) or kdialog (KDE)");
        return {};
    }
    std::string path = run(args);
    // Like lpstrDefExt: a name typed without an extension gets the default one
    if (kind == Kind::Save && !path.empty() && defaultExtension && *defaultExtension) {
        const size_t slash = path.rfind('/');
        const std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
        if (name.find('.') == std::string::npos) path += std::string(".") + defaultExtension;
    }
    return path;
}

} // namespace

std::string openFileDialog(void*, const std::string& initialPath, const std::string& initialFolder,
                           const FileFilter* filters, int filterCount) {
    return dialog(Kind::Open, initialPath, initialFolder, filters, filterCount, nullptr);
}

std::string saveFileDialog(void*, const std::string& initialPath, const std::string& initialFolder,
                           const FileFilter* filters, int filterCount, const char* defaultExtension) {
    return dialog(Kind::Save, initialPath, initialFolder, filters, filterCount, defaultExtension);
}

std::string pickFolderDialog(void*, const std::string& initialFolder) {
    return dialog(Kind::Folder, {}, initialFolder, nullptr, 0, nullptr);
}

} // namespace mealkit
