#pragma once

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include <string>

namespace utils {

std::wstring Utf8ToWide(const std::string& text);
std::string WideToUtf8(const std::wstring& text);

std::string GetExecutableDirectory();
std::string GetSettingsFilePath();
bool FileExists(const std::string& path);
bool FindExecutableInPath(const std::string& exeName, std::string* resolvedPath);

bool BrowseForFolder(HWND owner, std::string& path);
bool BrowseForExecutable(HWND owner, std::string& path, const wchar_t* title);
void OpenUrl(const std::string& url);
std::string FormatEta(int seconds);

} 