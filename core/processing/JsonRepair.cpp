#include "JsonRepair.h"

#include <algorithm>
#include <cctype>
#include <utility>

namespace local_jarvis::processing {
namespace {

std::string trim(const std::string &value)
{
    const auto first = std::find_if_not(value.begin(), value.end(), [](unsigned char character) {
        return std::isspace(character);
    });
    const auto last = std::find_if_not(value.rbegin(), value.rend(), [](unsigned char character) {
        return std::isspace(character);
    }).base();

    if (first >= last) {
        return {};
    }
    return { first, last };
}

std::string unescapeJsonString(const std::string &value)
{
    std::string output;
    output.reserve(value.size());

    for (std::size_t index = 0; index < value.size(); ++index) {
        if (value[index] != '\\' || index + 1 >= value.size()) {
            output.push_back(value[index]);
            continue;
        }

        const char escaped = value[++index];
        switch (escaped) {
        case 'n':
            output.push_back('\n');
            break;
        case 'r':
            output.push_back('\r');
            break;
        case 't':
            output.push_back('\t');
            break;
        case '"':
        case '\\':
        case '/':
            output.push_back(escaped);
            break;
        default:
            output.push_back(escaped);
            break;
        }
    }

    return output;
}

std::optional<std::pair<std::size_t, std::size_t>> findJsonValueRange(const std::string &json, const std::string &key)
{
    const std::string needle = "\"" + key + "\"";
    const std::size_t keyIndex = json.find(needle);
    if (keyIndex == std::string::npos) {
        return std::nullopt;
    }

    const std::size_t colon = json.find(':', keyIndex + needle.size());
    if (colon == std::string::npos) {
        return std::nullopt;
    }

    std::size_t start = colon + 1;
    while (start < json.size() && std::isspace(static_cast<unsigned char>(json[start]))) {
        ++start;
    }
    if (start >= json.size()) {
        return std::nullopt;
    }

    bool inString = false;
    bool escaped = false;
    int braceDepth = 0;
    int bracketDepth = 0;

    for (std::size_t index = start; index < json.size(); ++index) {
        const char character = json[index];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (character == '\\' && inString) {
            escaped = true;
            continue;
        }
        if (character == '"') {
            inString = !inString;
            continue;
        }
        if (inString) {
            continue;
        }

        if (character == '{') {
            ++braceDepth;
        } else if (character == '}') {
            if (braceDepth == 0 && bracketDepth == 0) {
                return std::make_pair(start, index);
            }
            --braceDepth;
        } else if (character == '[') {
            ++bracketDepth;
        } else if (character == ']') {
            --bracketDepth;
        } else if (character == ',' && braceDepth == 0 && bracketDepth == 0) {
            return std::make_pair(start, index);
        }
    }

    return std::make_pair(start, json.size());
}

} // namespace

std::optional<std::string> JsonRepair::extractJsonObject(const std::string &modelOutput)
{
    const std::size_t start = modelOutput.find('{');
    if (start == std::string::npos) {
        return std::nullopt;
    }

    bool inString = false;
    bool escaped = false;
    int depth = 0;
    for (std::size_t index = start; index < modelOutput.size(); ++index) {
        const char character = modelOutput[index];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (character == '\\' && inString) {
            escaped = true;
            continue;
        }
        if (character == '"') {
            inString = !inString;
            continue;
        }
        if (inString) {
            continue;
        }
        if (character == '{') {
            ++depth;
        } else if (character == '}') {
            --depth;
            if (depth == 0) {
                return modelOutput.substr(start, index - start + 1);
            }
        }
    }

    return std::nullopt;
}

bool JsonRepair::isValidJsonObject(const std::string &json)
{
    const std::string value = trim(json);
    if (value.size() < 2 || value.front() != '{' || value.back() != '}') {
        return false;
    }

    bool inString = false;
    bool escaped = false;
    int objectDepth = 0;
    int arrayDepth = 0;

    for (const char character : value) {
        if (escaped) {
            escaped = false;
            continue;
        }
        if (character == '\\' && inString) {
            escaped = true;
            continue;
        }
        if (character == '"') {
            inString = !inString;
            continue;
        }
        if (inString) {
            continue;
        }
        if (character == '{') {
            ++objectDepth;
        } else if (character == '}') {
            --objectDepth;
            if (objectDepth < 0) {
                return false;
            }
        } else if (character == '[') {
            ++arrayDepth;
        } else if (character == ']') {
            --arrayDepth;
            if (arrayDepth < 0) {
                return false;
            }
        }
    }

    return !inString && objectDepth == 0 && arrayDepth == 0;
}

std::optional<std::string> JsonRepair::normalizeJsonObject(const std::string &modelOutput)
{
    const auto json = extractJsonObject(modelOutput);
    if (!json.has_value() || !isValidJsonObject(*json)) {
        return std::nullopt;
    }

    return trim(*json);
}

std::optional<std::string> JsonRepair::extractString(const std::string &json, const std::string &key)
{
    const auto range = findJsonValueRange(json, key);
    if (!range.has_value()) {
        return std::nullopt;
    }

    const std::string raw = trim(json.substr(range->first, range->second - range->first));
    if (raw.size() < 2 || raw.front() != '"') {
        return std::nullopt;
    }

    std::string value;
    bool escaped = false;
    for (std::size_t index = 1; index < raw.size(); ++index) {
        const char character = raw[index];
        if (escaped) {
            value.push_back('\\');
            value.push_back(character);
            escaped = false;
            continue;
        }
        if (character == '\\') {
            escaped = true;
            continue;
        }
        if (character == '"') {
            return unescapeJsonString(value);
        }
        value.push_back(character);
    }

    return std::nullopt;
}

std::vector<std::string> JsonRepair::extractStringArray(const std::string &json, const std::string &key)
{
    std::vector<std::string> values;
    const auto range = findJsonValueRange(json, key);
    if (!range.has_value()) {
        return values;
    }

    const std::string raw = trim(json.substr(range->first, range->second - range->first));
    if (raw.size() < 2 || raw.front() != '[' || raw.back() != ']') {
        return values;
    }

    bool inString = false;
    bool escaped = false;
    std::string current;
    for (std::size_t index = 1; index + 1 < raw.size(); ++index) {
        const char character = raw[index];
        if (escaped) {
            current.push_back('\\');
            current.push_back(character);
            escaped = false;
            continue;
        }
        if (character == '\\' && inString) {
            escaped = true;
            continue;
        }
        if (character == '"') {
            if (inString) {
                values.push_back(unescapeJsonString(current));
                current.clear();
            }
            inString = !inString;
            continue;
        }
        if (inString) {
            current.push_back(character);
        }
    }

    return values;
}

std::vector<std::string> JsonRepair::extractObjectArray(const std::string &json, const std::string &key)
{
    std::vector<std::string> objects;
    const auto range = findJsonValueRange(json, key);
    if (!range.has_value()) {
        return objects;
    }

    const std::string raw = trim(json.substr(range->first, range->second - range->first));
    if (raw.size() < 2 || raw.front() != '[' || raw.back() != ']') {
        return objects;
    }

    bool inString = false;
    bool escaped = false;
    int depth = 0;
    std::size_t objectStart = std::string::npos;

    for (std::size_t index = 1; index + 1 < raw.size(); ++index) {
        const char character = raw[index];
        if (escaped) {
            escaped = false;
            continue;
        }
        if (character == '\\' && inString) {
            escaped = true;
            continue;
        }
        if (character == '"') {
            inString = !inString;
            continue;
        }
        if (inString) {
            continue;
        }
        if (character == '{') {
            if (depth == 0) {
                objectStart = index;
            }
            ++depth;
        } else if (character == '}') {
            --depth;
            if (depth == 0 && objectStart != std::string::npos) {
                objects.push_back(raw.substr(objectStart, index - objectStart + 1));
                objectStart = std::string::npos;
            }
        }
    }

    return objects;
}

} // namespace local_jarvis::processing
