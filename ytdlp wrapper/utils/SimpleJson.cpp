#include "SimpleJson.h"

#include <cctype>
#include <iomanip>
#include <sstream>

namespace {

std::size_t FindValueStart(const std::string& json, const std::string& key) {
    const std::size_t keyPos = json.find("\"" + key + "\"");
    if (keyPos == std::string::npos) {
        return std::string::npos;
    }

    std::size_t pos = json.find(':', keyPos);
    if (pos == std::string::npos) {
        return std::string::npos;
    }

    ++pos;
    while (pos < json.size() && std::isspace(static_cast<unsigned char>(json[pos])) != 0) {
        ++pos;
    }
    return pos;
}

} 
namespace utils::simplejson {

std::string EscapeString(const std::string& value) {
    std::string output;
    output.reserve(value.size());

    for (const char ch : value) {
        switch (ch) {
        case '\\': output += "\\\\"; break;
        case '"': output += "\\\""; break;
        case '\n': output += "\\n"; break;
        case '\r': output += "\\r"; break;
        case '\t': output += "\\t"; break;
        default: output += ch; break;
        }
    }

    return output;
}

std::string BoolToString(const bool value) {
    return value ? "true" : "false";
}

std::string NumberToString(const double value) {
    std::ostringstream stream;
    stream << std::fixed << std::setprecision(3) << value;
    return stream.str();
}

bool TryGetString(const std::string& json, const std::string& key, std::string& value) {
    std::size_t pos = FindValueStart(json, key);
    if (pos == std::string::npos || pos >= json.size() || json[pos] != '"') {
        return false;
    }

    ++pos;
    std::string result;
    while (pos < json.size()) {
        const char ch = json[pos++];
        if (ch == '\\' && pos < json.size()) {
            const char escaped = json[pos++];
            switch (escaped) {
            case '\\': result += '\\'; break;
            case '"': result += '"'; break;
            case 'n': result += '\n'; break;
            case 'r': result += '\r'; break;
            case 't': result += '\t'; break;
            default: result += escaped; break;
            }
            continue;
        }
        if (ch == '"') {
            value = result;
            return true;
        }
        result += ch;
    }

    return false;
}

bool TryGetBool(const std::string& json, const std::string& key, bool& value) {
    const std::size_t pos = FindValueStart(json, key);
    if (pos == std::string::npos) {
        return false;
    }

    if (json.compare(pos, 4, "true") == 0) {
        value = true;
        return true;
    }
    if (json.compare(pos, 5, "false") == 0) {
        value = false;
        return true;
    }
    return false;
}

bool TryGetNumber(const std::string& json, const std::string& key, int& value) {
    const std::size_t pos = FindValueStart(json, key);
    if (pos == std::string::npos) {
        return false;
    }

    std::size_t end = pos;
    while (end < json.size() &&
        (std::isdigit(static_cast<unsigned char>(json[end])) != 0 || json[end] == '-' || json[end] == '+')) {
        ++end;
    }

    if (end == pos) {
        return false;
    }

    value = std::stoi(json.substr(pos, end - pos));
    return true;
}

bool TryGetNumber(const std::string& json, const std::string& key, float& value) {
    const std::size_t pos = FindValueStart(json, key);
    if (pos == std::string::npos) {
        return false;
    }

    std::size_t end = pos;
    while (end < json.size() &&
        (std::isdigit(static_cast<unsigned char>(json[end])) != 0 ||
            json[end] == '-' || json[end] == '+' || json[end] == '.')) {
        ++end;
    }

    if (end == pos) {
        return false;
    }

    value = std::stof(json.substr(pos, end - pos));
    return true;
}

} 