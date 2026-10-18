#include "mealkit/platform/FileDialog.h"

#include <windows.h>
#include <commdlg.h>
#include <shobjidl.h>

namespace mealkit {
namespace {

std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}

std::string narrow(const wchar_t* w) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    std::string s(static_cast<size_t>(n > 0 ? n - 1 : 0), '\0');
    if (n > 1) WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}

// "이름\0패턴\0...\0\0" 형식
std::wstring filterString(const FileFilter* filters, int count) {
    std::wstring out;
    for (int i = 0; i < count; ++i) {
        out += widen(filters[i].name);
        out.push_back(L'\0');
        out += widen(filters[i].patterns);
        out.push_back(L'\0');
    }
    out.push_back(L'\0');
    return out;
}

std::string fileDialog(void* owner, bool save, const std::string& initialPath, const std::string& initialFolder,
                       const FileFilter* filters, int count, const char* defaultExtension) {
    wchar_t buffer[MAX_PATH] = {};
    const std::wstring initial = widen(initialPath);
    const std::wstring folder = widen(initialFolder);
    const std::wstring filter = filterString(filters, count);
    const std::wstring extension = defaultExtension ? widen(defaultExtension) : std::wstring();
    wcsncpy_s(buffer, initial.c_str(), _TRUNCATE);
    OPENFILENAMEW ofn{};
    ofn.lStructSize = sizeof(ofn);
    ofn.hwndOwner = static_cast<HWND>(owner);
    ofn.lpstrFilter = count > 0 ? filter.c_str() : nullptr;
    ofn.lpstrFile = buffer;
    ofn.nMaxFile = MAX_PATH;
    ofn.lpstrDefExt = extension.empty() ? nullptr : extension.c_str();
    ofn.lpstrInitialDir = folder.empty() ? nullptr : folder.c_str();
    ofn.Flags = OFN_NOCHANGEDIR | (save ? OFN_OVERWRITEPROMPT : OFN_FILEMUSTEXIST);
    const BOOL ok = save ? GetSaveFileNameW(&ofn) : GetOpenFileNameW(&ofn);
    return ok ? narrow(buffer) : std::string();
}

} // namespace

std::string openFileDialog(void* owner, const std::string& initialPath, const std::string& initialFolder,
                           const FileFilter* filters, int filterCount) {
    return fileDialog(owner, false, initialPath, initialFolder, filters, filterCount, nullptr);
}

std::string saveFileDialog(void* owner, const std::string& initialPath, const std::string& initialFolder,
                           const FileFilter* filters, int filterCount, const char* defaultExtension) {
    return fileDialog(owner, true, initialPath, initialFolder, filters, filterCount, defaultExtension);
}

std::string pickFolderDialog(void* owner, const std::string& initialFolder) {
    CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
    std::string result;
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) return {};
    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST | FOS_PICKFOLDERS);
    if (!initialFolder.empty()) {
        IShellItem* folder = nullptr;
        if (SUCCEEDED(SHCreateItemFromParsingName(widen(initialFolder).c_str(), nullptr, IID_PPV_ARGS(&folder)))) {
            dialog->SetFolder(folder);
            folder->Release();
        }
    }
    if (SUCCEEDED(dialog->Show(static_cast<HWND>(owner)))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR path = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &path))) {
                result = narrow(path);
                CoTaskMemFree(path);
            }
            item->Release();
        }
    }
    dialog->Release();
    return result;
}

} // namespace mealkit
