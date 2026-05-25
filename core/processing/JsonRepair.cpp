#include "JsonRepair.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>

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

std::optional<nlohmann::json> parseJsonObject(const std::string &jsonText)
{
    const auto json = nlohmann::json::parse(jsonText, nullptr, false);
    if (json.is_discarded() || !json.is_object()) {
        return std::nullopt;
    }
    return std::optional<nlohmann::json>(json);
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
    return parseJsonObject(trim(json)).has_value();
}

std::optional<std::string> JsonRepair::normalizeJsonObject(const std::string &modelOutput)
{
    const auto jsonText = extractJsonObject(modelOutput);
    if (!jsonText.has_value()) {
        return std::nullopt;
    }

    const auto json = parseJsonObject(trim(*jsonText));
    if (!json.has_value()) {
        return std::nullopt;
    }

    return json->dump();
}

std::optional<std::string> JsonRepair::extractString(const std::string &json, const std::string &key)
{
    const auto parsed = parseJsonObject(json);
    if (!parsed.has_value()) {
        return std::nullopt;
    }

    const auto iterator = parsed->find(key);
    if (iterator == parsed->end() || !iterator->is_string()) {
        return std::nullopt;
    }

    return iterator->get<std::string>();
}

std::vector<std::string> JsonRepair::extractStringArray(const std::string &json, const std::string &key)
{
    std::vector<std::string> values;
    const auto parsed = parseJsonObject(json);
    if (!parsed.has_value()) {
        return values;
    }

    const auto iterator = parsed->find(key);
    if (iterator == parsed->end() || !iterator->is_array()) {
        return values;
    }

    for (const auto &value : *iterator) {
        if (value.is_string()) {
            values.push_back(value.get<std::string>());
        }
    }
    return values;
}

std::vector<std::string> JsonRepair::extractObjectArray(const std::string &json, const std::string &key)
{
    std::vector<std::string> objects;
    const auto parsed = parseJsonObject(json);
    if (!parsed.has_value()) {
        return objects;
    }

    const auto iterator = parsed->find(key);
    if (iterator == parsed->end() || !iterator->is_array()) {
        return objects;
    }

    for (const auto &value : *iterator) {
        if (value.is_object()) {
            objects.push_back(value.dump());
        }
    }
    return objects;
}

} // namespace local_jarvis::processing
