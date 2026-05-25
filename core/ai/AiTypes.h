#pragma once

#include <functional>
#include <string>
#include <vector>

namespace local_jarvis::ai {

inline constexpr const char *kOllamaHost = "localhost";
inline constexpr int kOllamaPort = 11434;
inline constexpr const char *kDefaultGemmaModel = "gemma4:e4b";
inline constexpr const char *kFallbackGemmaModel = "gemma4:e2b";

struct ChatMessage {
    std::string role;
    std::string content;
};

struct ModelReadiness {
    bool ollamaRunning = false;
    bool modelInstalled = false;
    std::string recommendedModel;
    std::string selectedModel;
    std::string message;
};

using ProgressCallback = std::function<void(const std::string &)>;

} // namespace local_jarvis::ai
