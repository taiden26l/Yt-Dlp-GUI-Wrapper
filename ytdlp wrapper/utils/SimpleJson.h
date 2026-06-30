#pragma once

#include <string>

namespace utils::simplejson {

std::string EscapeString(const std::string& value);
std::string BoolToString(bool value);
std::string NumberToString(double value);

bool TryGetString(const std::string& json, const std::string& key, std::string& value);
bool TryGetBool(const std::string& json, const std::string& key, bool& value);
bool TryGetNumber(const std::string& json, const std::string& key, int& value);
bool TryGetNumber(const std::string& json, const std::string& key, float& value);

} 