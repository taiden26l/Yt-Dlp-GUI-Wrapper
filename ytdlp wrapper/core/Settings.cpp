#include "Settings.h"

#include "../utils/SimpleJson.h"
#include "../utils/SystemPaths.h"

#include <fstream>
#include <sstream>

namespace core {

AppSettings SettingsStore::Load() {
    AppSettings settings;

    std::ifstream input(GetSettingsPath(), std::ios::binary);
    if (!input) {
        return settings;
    }

    std::ostringstream buffer;
    buffer << input.rdbuf();
    const std::string json = buffer.str();

    utils::simplejson::TryGetString(json, "outputFolder", settings.outputFolder);
    utils::simplejson::TryGetNumber(json, "defaultMode", settings.defaultMode);
    utils::simplejson::TryGetBool(json, "embedMetadata", settings.embedMetadata);
    utils::simplejson::TryGetBool(json, "embedThumbnail", settings.embedThumbnail);
    utils::simplejson::TryGetBool(json, "downloadSubs", settings.downloadSubs);
    utils::simplejson::TryGetString(json, "ytDlpPath", settings.ytDlpPath);
    utils::simplejson::TryGetNumber(json, "accentR", settings.accentColor.r);
    utils::simplejson::TryGetNumber(json, "accentG", settings.accentColor.g);
    utils::simplejson::TryGetNumber(json, "accentB", settings.accentColor.b);
    utils::simplejson::TryGetNumber(json, "accentA", settings.accentColor.a);

    return settings;
}

bool SettingsStore::Save(const AppSettings& settings) {
    const std::string payload =
        "{\n"
        "  \"outputFolder\": \"" + utils::simplejson::EscapeString(settings.outputFolder) + "\",\n"
        "  \"defaultMode\": " + std::to_string(settings.defaultMode) + ",\n"
        "  \"embedMetadata\": " + utils::simplejson::BoolToString(settings.embedMetadata) + ",\n"
        "  \"embedThumbnail\": " + utils::simplejson::BoolToString(settings.embedThumbnail) + ",\n"
        "  \"downloadSubs\": " + utils::simplejson::BoolToString(settings.downloadSubs) + ",\n"
        "  \"ytDlpPath\": \"" + utils::simplejson::EscapeString(settings.ytDlpPath) + "\",\n"
        "  \"accentR\": " + utils::simplejson::NumberToString(settings.accentColor.r) + ",\n"
        "  \"accentG\": " + utils::simplejson::NumberToString(settings.accentColor.g) + ",\n"
        "  \"accentB\": " + utils::simplejson::NumberToString(settings.accentColor.b) + ",\n"
        "  \"accentA\": " + utils::simplejson::NumberToString(settings.accentColor.a) + "\n"
        "}\n";

    std::ofstream output(GetSettingsPath(), std::ios::binary | std::ios::trunc);
    if (!output) {
        return false;
    }

    output << payload;
    return static_cast<bool>(output);
}

std::string SettingsStore::GetSettingsPath() {
    return utils::GetSettingsFilePath();
}

} 