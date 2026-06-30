#pragma once

#include <string>

namespace core {

struct AccentColor {
    float r = 0.96f;
    float g = 0.26f;
    float b = 0.21f;
    float a = 1.0f;
};

struct AppSettings {
    std::string outputFolder;
    int defaultMode = 0;
    bool embedMetadata = true;
    bool embedThumbnail = false;
    bool downloadSubs = false;
    AccentColor accentColor;
    std::string ytDlpPath;
};

class SettingsStore {
public:
    static AppSettings Load();
    static bool Save(const AppSettings& settings);
    static std::string GetSettingsPath();
};

} 