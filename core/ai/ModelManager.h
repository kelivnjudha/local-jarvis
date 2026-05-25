#pragma once

#include "AiTypes.h"
#include "OllamaClient.h"
#include "setup/SystemCheck.h"

#include <string>

namespace local_jarvis::ai {

class ModelManager {
public:
    ModelManager(OllamaClient &ollamaClient, const setup::SystemCheck &systemCheck);

    [[nodiscard]] std::string detectRecommendedModel() const;
    [[nodiscard]] ModelReadiness ensureModelReady(const std::string &preferredModel = {});
    bool pullModel(const std::string &modelName, const ProgressCallback &progressCallback);

private:
    [[nodiscard]] bool isSafeModelName(const std::string &modelName) const;

    OllamaClient &m_ollamaClient;
    const setup::SystemCheck &m_systemCheck;
};

} // namespace local_jarvis::ai
