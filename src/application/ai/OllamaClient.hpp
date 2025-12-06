#pragma once

#include "ai/IAIService.hpp"
#include <string>
#include <vector>
#include <memory>

namespace ai
{

/**
 * @brief Information about an installed Ollama model
 */
struct ModelInfo
{
    std::string name;
    std::string size;
    std::string modified;
};

/**
 * @brief Client for communicating with local Ollama LLM server
 * 
 * Ollama runs locally and provides OpenAI-compatible API.
 * Default endpoint: http://localhost:11434
 */
class OllamaClient : public IAIService
{
public:
    explicit OllamaClient(const std::string& model = "llama3.2",
                         const std::string& baseUrl = "http://localhost:11434");
    ~OllamaClient() override = default;

    std::string SendPrompt(const std::string& prompt,
        std::function<void(const std::string&)> callback = nullptr) override;

    bool IsAvailable() const override;
    std::string GetModelName() const override;

    void SetModel(const std::string& model);
    void SetBaseUrl(const std::string& url);
    
    /**
     * @brief Get list of installed models from Ollama
     * @return Vector of model information
     */
    std::vector<ModelInfo> GetInstalledModels() const;

private:
    std::string m_model;
    std::string m_baseUrl;

    std::string SendHttpPost(const std::string& endpoint,
                             const std::string& jsonBody) const;
    std::string SendHttpGet(const std::string& endpoint) const;
};

} // namespace ai
