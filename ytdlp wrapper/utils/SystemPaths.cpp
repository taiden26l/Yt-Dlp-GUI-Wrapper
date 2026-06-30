#include "SystemPaths.h"

#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <shobjidl_core.h>

#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace utils {

std::wstring Utf8ToWide(const std::string& text) {
    if (text.empty()) {
        return {};
    }

    const int size = MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, nullptr, 0);
    std::wstring result(static_cast<std::size_t>(size), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, text.c_str(), -1, result.data(), size);
    if (!result.empty() && result.back() == L'\0') {
        result.pop_back();
    }
    return result;
}

std::string WideToUtf8(const std::wstring& text) {
    if (text.empty()) {
        return {};
    }

    const int size = WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, nullptr, 0, nullptr, nullptr);
    std::string result(static_cast<std::size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text.c_str(), -1, result.data(), size, nullptr, nullptr);
    if (!result.empty() && result.back() == '\0') {
        result.pop_back();
    }
    return result;
}

std::string GetExecutableDirectory() {
    std::vector<wchar_t> buffer(MAX_PATH, L'\0');
    DWORD copied = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    while (copied >= buffer.size() - 1) {
        buffer.resize(buffer.size() * 2, L'\0');
        copied = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    }

    return fs::path(buffer.data()).parent_path().u8string();
}

std::string GetSettingsFilePath() {
    PWSTR roaming = nullptr;
    std::string basePath;

    if (SUCCEEDED(SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, nullptr, &roaming))) {
        basePath = WideToUtf8(roaming);
        CoTaskMemFree(roaming);
    } else {
        basePath = GetExecutableDirectory();
    }

    fs::path settingsDir = fs::u8path(basePath) / "YtDlpWrapper";
    std::error_code ec;
    fs::create_directories(settingsDir, ec);
    return (settingsDir / "settings.json").u8string();
}

bool FileExists(const std::string& path) {
    if (path.empty()) {
        return false;
    }

    std::error_code ec;
    return fs::exists(fs::u8path(path), ec);
}

bool FindExecutableInPath(const std::string& exeName, std::string* resolvedPath) {
    if (FileExists(exeName)) {
        if (resolvedPath != nullptr) {
            *resolvedPath = exeName;
        }
        return true;
    }

    if (FileExists(exeName + ".exe")) {
        if (resolvedPath != nullptr) {
            *resolvedPath = exeName + ".exe";
        }
        return true;
    }

    std::vector<char> buffer(MAX_PATH, '\0');
    DWORD result = SearchPathA(nullptr, exeName.c_str(), ".exe", static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
    while (result > buffer.size()) {
        buffer.resize(result + 1, '\0');
        result = SearchPathA(nullptr, exeName.c_str(), ".exe", static_cast<DWORD>(buffer.size()), buffer.data(), nullptr);
    }

    if (result == 0) {
        return false;
    }

    if (resolvedPath != nullptr) {
        *resolvedPath = buffer.data();
    }
    return true;
}

bool BrowseForFolder(HWND owner, std::string& path) {
    IFileOpenDialog* dialog = nullptr;
    if (FAILED(CoCreateInstance(CLSID_FileOpenDialog, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&dialog)))) {
        return false;
    }

    DWORD options = 0;
    dialog->GetOptions(&options);
    dialog->SetOptions(options | FOS_PICKFOLDERS | FOS_FORCEFILESYSTEM | FOS_PATHMUSTEXIST);
    dialog->SetTitle(L"Select output folder");

    bool selected = false;
    if (SUCCEEDED(dialog->Show(owner))) {
        IShellItem* item = nullptr;
        if (SUCCEEDED(dialog->GetResult(&item))) {
            PWSTR displayName = nullptr;
            if (SUCCEEDED(item->GetDisplayName(SIGDN_FILESYSPATH, &displayName))) {
                path = WideToUtf8(displayName);
                CoTaskMemFree(displayName);
                selected = true;
            }
            item->Release();
        }
    }

    dialog->Release();
    return selected;
}

bool BrowseForExecutable(HWND owner, std::string& path, const wchar_t* title) {
    std::vector<char> fileBuffer(MAX_PATH, '\0');
    OPENFILENAMEA dialog{};
    dialog.lStructSize = sizeof(dialog);
    dialog.hwndOwner = owner;
    dialog.lpstrFilter = "Executable\0*.exe\0All Files\0*.*\0";
    dialog.lpstrFile = fileBuffer.data();
    dialog.nMaxFile = static_cast<DWORD>(fileBuffer.size());
    dialog.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;

    std::string titleUtf8;
    if (title != nullptr) {
        titleUtf8 = WideToUtf8(title);
        dialog.lpstrTitle = titleUtf8.c_str();
    }

    if (!GetOpenFileNameA(&dialog)) {
        return false;
    }

    path = fileBuffer.data();
    return true;
}

void OpenUrl(const std::string& url) {
    ShellExecuteW(nullptr, L"open", Utf8ToWide(url).c_str(), nullptr, nullptr, SW_SHOW);
}

std::string FormatEta(const int seconds) {
    if (seconds < 0) {
        return "--";
    }

    const int hours = seconds / 3600;
    const int minutes = (seconds % 3600) / 60;
    const int remainder = seconds % 60;

    char buffer[32] = {};
    if (hours > 0) {
        sprintf_s(buffer, "%02d:%02d:%02d", hours, minutes, remainder);
    } else {
        sprintf_s(buffer, "%02d:%02d", minutes, remainder);
    }
    return buffer;
}

} 