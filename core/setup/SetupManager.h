#pragma once

#include "ai/ModelManager.h"
#include "ai/OllamaClient.h"
#include "storage/Storage.h"

#include <string>

namespace local_jarvis::setup {

struct SetupStatus {
    bool databaseReady = false;
    bool ollamaRunning = false;
    bool modelReady = false;
    std::string recommendedModel;
    std::string currentModel;
    std::string message;
};

class SetupManager {
public:
    SetupManager(
        storage::Storage &storage,
        ai::ModelManager &modelManager,
        ai::OllamaClient &ollamaClient);

    SetupStatus firstRunCheck();
    SetupStatus setupLocalAi(const ai::ProgressCallback &progressCallback);
    bool setupDatabase();

private:
    void persistSetupStatus(const SetupStatus &status);
    std::string configuredModelOrRecommended();

    storage::Storage &m_storage;
    ai::ModelManager &m_modelManager;
    ai::OllamaClient &m_ollamaClient;
};

} // namespace local_jarvis::setup
