#include "SetupManager.h"

#include "ai/AiTypes.h"
#include "ai/PromptBuilder.h"

#include <algorithm>
#include <cctype>
#include <filesystem>

namespace local_jarvis::setup {
namespace {

std::string boolText(bool value)
{
    return value ? "true" : "false";
}

std::string trim(std::string value)
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

} // namespace

SetupManager::SetupManager(
    storage::Storage &storage,
    ai::ModelManager &modelManager,
    ai::OllamaClient &ollamaClient)
    : m_storage(storage)
    , m_modelManager(modelManager)
    , m_ollamaClient(ollamaClient)
{
}

SetupStatus SetupManager::firstRunCheck()
{
    SetupStatus status;
    status.databaseReady = setupDatabase();
    status.recommendedModel = m_modelManager.detectRecommendedModel();
    status.currentModel = configuredModelOrRecommended();

    const auto modelStatus = m_modelManager.ensureModelReady(status.currentModel);
    status.ollamaRunning = modelStatus.ollamaRunning;
    status.modelReady = modelStatus.modelInstalled;
    status.message = status.databaseReady ? modelStatus.message : m_storage.lastError();

    persistSetupStatus(status);
    return status;
}

SetupStatus SetupManager::setupLocalAi(const ai::ProgressCallback &progressCallback)
{
    SetupStatus status;
    status.databaseReady = setupDatabase();
    status.recommendedModel = m_modelManager.detectRecommendedModel();
    status.currentModel = status.recommendedModel;

    if (progressCallback) {
        progressCallback("Checking local Ollama at http://localhost:11434");
    }

    auto modelStatus = m_modelManager.ensureModelReady(status.currentModel);
    status.ollamaRunning = modelStatus.ollamaRunning;

    if (!status.databaseReady) {
        status.message = m_storage.lastError();
        persistSetupStatus(status);
        return status;
    }

    if (!status.ollamaRunning) {
        status.message = "Ollama is not running. Start Ollama locally, then retry setup.";
        persistSetupStatus(status);
        return status;
    }

    if (!modelStatus.modelInstalled) {
        if (progressCallback) {
            progressCallback("Model missing locally. Pulling " + status.currentModel);
        }

        if (!m_modelManager.pullModel(status.currentModel, progressCallback)) {
            status.message = "Model pull failed for " + status.currentModel;
            m_storage.addModelEvent("pull_failed", status.currentModel, status.message);
            persistSetupStatus(status);
            return status;
        }

        m_storage.addModelEvent("pull_completed", status.currentModel, "User-triggered local model pull completed.");
    }

    std::string output;
    if (progressCallback) {
        progressCallback("Running local health check prompt.");
    }

    if (!m_ollamaClient.generate(status.currentModel, ai::PromptBuilder::healthCheckPrompt(), output)) {
        status.message = "Local AI health check failed: " + m_ollamaClient.lastError();
        m_storage.addModelEvent("health_check_failed", status.currentModel, status.message);
        persistSetupStatus(status);
        return status;
    }

    status.modelReady = trim(output) == "LOCAL_JARVIS_READY";
    status.message = status.modelReady
        ? "Local AI is ready."
        : "Health check returned unexpected output: " + output;

    if (status.modelReady) {
        m_storage.setSetting("ai.current_model", status.currentModel);
        m_storage.addModelEvent("health_check_passed", status.currentModel, "Local model replied with LOCAL_JARVIS_READY.");
    } else {
        m_storage.addModelEvent("health_check_unexpected_output", status.currentModel, status.message);
    }

    persistSetupStatus(status);
    return status;
}

bool SetupManager::setupDatabase()
{
    if (m_storage.isOpen()) {
        if (!m_storage.createSchema()) {
            return false;
        }
    } else if (!m_storage.initialize()) {
        return false;
    }

    const std::string recommendedModel = m_modelManager.detectRecommendedModel();
    if (!m_storage.getSetting("ai.current_model").has_value()) {
        m_storage.setSetting("ai.current_model", recommendedModel);
    }
    m_storage.setSetting("setup.database_ready", "true");
    return true;
}

void SetupManager::persistSetupStatus(const SetupStatus &status)
{
    if (!m_storage.isOpen()) {
        return;
    }

    m_storage.setSetting("setup.database_ready", boolText(status.databaseReady));
    m_storage.setSetting("setup.ollama_running", boolText(status.ollamaRunning));
    m_storage.setSetting("setup.model_ready", boolText(status.modelReady));
    m_storage.setSetting("setup.completed", boolText(status.databaseReady && status.modelReady));
    m_storage.setSetting("ai.recommended_model", status.recommendedModel);
    m_storage.setSetting("setup.last_message", status.message);
}

std::string SetupManager::configuredModelOrRecommended()
{
    const auto configured = m_storage.getSetting("ai.current_model");
    if (configured.has_value() && !configured->empty()) {
        return *configured;
    }

    return m_modelManager.detectRecommendedModel();
}

} // namespace local_jarvis::setup
