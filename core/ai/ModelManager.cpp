#include "ModelManager.h"

#include <array>
#include <cctype>
#include <cstdio>
#include <string>

namespace local_jarvis::ai {
namespace {

#if defined(_WIN32)
#define LOCAL_JARVIS_POPEN _popen
#define LOCAL_JARVIS_PCLOSE _pclose
#else
#define LOCAL_JARVIS_POPEN popen
#define LOCAL_JARVIS_PCLOSE pclose
#endif

std::string shellQuote(const std::string &value)
{
#if defined(_WIN32)
    return "\"" + value + "\"";
#else
    return "'" + value + "'";
#endif
}

} // namespace

ModelManager::ModelManager(OllamaClient &ollamaClient, const setup::SystemCheck &systemCheck)
    : m_ollamaClient(ollamaClient)
    , m_systemCheck(systemCheck)
{
}

std::string ModelManager::detectRecommendedModel() const
{
    return m_systemCheck.totalRamGb() >= 16.0 ? kDefaultGemmaModel : kFallbackGemmaModel;
}

ModelReadiness ModelManager::ensureModelReady(const std::string &preferredModel)
{
    ModelReadiness status;
    status.recommendedModel = detectRecommendedModel();
    status.selectedModel = preferredModel.empty() ? status.recommendedModel : preferredModel;

    status.ollamaRunning = m_ollamaClient.isOllamaRunning();
    if (!status.ollamaRunning) {
        status.message = "Ollama is not running at http://localhost:11434.";
        return status;
    }

    status.modelInstalled = m_ollamaClient.isModelInstalled(status.selectedModel);
    if (!status.modelInstalled) {
        status.message = "Model is not installed locally: " + status.selectedModel;
        return status;
    }

    status.message = "Local model is ready: " + status.selectedModel;
    return status;
}

bool ModelManager::pullModel(const std::string &modelName, const ProgressCallback &progressCallback)
{
    if (!isSafeModelName(modelName)) {
        if (progressCallback) {
            progressCallback("Refusing to pull model with unsafe name: " + modelName);
        }
        return false;
    }

    const std::string command = "ollama pull " + shellQuote(modelName) + " 2>&1";
    if (progressCallback) {
        progressCallback("Running: ollama pull " + modelName);
    }

    FILE *pipe = LOCAL_JARVIS_POPEN(command.c_str(), "r");
    if (pipe == nullptr) {
        if (progressCallback) {
            progressCallback("Failed to launch ollama pull. Make sure Ollama is installed and on PATH.");
        }
        return false;
    }

    std::array<char, 512> buffer {};
    while (fgets(buffer.data(), static_cast<int>(buffer.size()), pipe) != nullptr) {
        if (progressCallback) {
            progressCallback(std::string(buffer.data()));
        }
    }

    const int exitCode = LOCAL_JARVIS_PCLOSE(pipe);
    if (exitCode != 0) {
        if (progressCallback) {
            progressCallback("ollama pull exited with code " + std::to_string(exitCode));
        }
        return false;
    }

    if (progressCallback) {
        progressCallback("Model pull completed: " + modelName);
    }
    return true;
}

bool ModelManager::isSafeModelName(const std::string &modelName) const
{
    if (modelName.empty()) {
        return false;
    }

    for (const unsigned char character : modelName) {
        if (std::isalnum(character) || character == ':' || character == '-' || character == '_' || character == '.') {
            continue;
        }
        return false;
    }

    return true;
}

} // namespace local_jarvis::ai
