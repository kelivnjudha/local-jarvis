#pragma once

#include <optional>
#include <string>
#include <vector>

namespace local_jarvis::processing {

class JsonRepair {
public:
    [[nodiscard]] static std::optional<std::string> extractJsonObject(const std::string &modelOutput);
    [[nodiscard]] static bool isValidJsonObject(const std::string &json);
    [[nodiscard]] static std::optional<std::string> normalizeJsonObject(const std::string &modelOutput);
    [[nodiscard]] static std::optional<std::string> extractString(const std::string &json, const std::string &key);
    [[nodiscard]] static std::vector<std::string> extractStringArray(const std::string &json, const std::string &key);
    [[nodiscard]] static std::vector<std::string> extractObjectArray(const std::string &json, const std::string &key);
};

} // namespace local_jarvis::processing
