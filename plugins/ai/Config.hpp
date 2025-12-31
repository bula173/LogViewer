// Plugin-local configuration for the AI plugin
#pragma once

#include <string>
#include <filesystem>
#include <nlohmann/json.hpp>

namespace config {

struct Config {
    std::string aiProvider = "ollama";
    std::string ollamaBaseUrl = "http://localhost:11434";
    std::string openaiApiKey;
    std::string anthropicApiKey;
    std::string googleApiKey;
    std::string xaiApiKey;
    std::string ollamaDefaultModel;
    int aiTimeoutSeconds = 30;

    void ApplyJson(const nlohmann::json& j) {
        if (j.contains("aiTimeoutSeconds") && j["aiTimeoutSeconds"].is_number())
            aiTimeoutSeconds = j["aiTimeoutSeconds"].get<int>();
        if (j.contains("provider") && j["provider"].is_string())
            aiProvider = j["provider"].get<std::string>();
        if (j.contains("baseUrl") && j["baseUrl"].is_string())
            ollamaBaseUrl = j["baseUrl"].get<std::string>();
        if (j.contains("model") && j["model"].is_string())
            ollamaDefaultModel = j["model"].get<std::string>();
        if (j.contains("openaiApiKey") && j["openaiApiKey"].is_string())
            openaiApiKey = j["openaiApiKey"].get<std::string>();
        if (j.contains("anthropicApiKey") && j["anthropicApiKey"].is_string())
            anthropicApiKey = j["anthropicApiKey"].get<std::string>();
        if (j.contains("googleApiKey") && j["googleApiKey"].is_string())
            googleApiKey = j["googleApiKey"].get<std::string>();
        if (j.contains("xaiApiKey") && j["xaiApiKey"].is_string())
            xaiApiKey = j["xaiApiKey"].get<std::string>();
    }

    nlohmann::json ToJson() const {
        nlohmann::json j;
        j["provider"] = aiProvider;
        j["baseUrl"] = ollamaBaseUrl;
        j["model"] = ollamaDefaultModel;
        j["aiTimeoutSeconds"] = aiTimeoutSeconds;
        j["openaiApiKey"] = openaiApiKey;
        j["anthropicApiKey"] = anthropicApiKey;
        j["googleApiKey"] = googleApiKey;
        j["xaiApiKey"] = xaiApiKey;
        return j;
    }
};

inline Config& GetConfig() {
    static Config cfg;
    return cfg;
}

} // namespace config
