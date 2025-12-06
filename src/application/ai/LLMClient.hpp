#pragma once

#include "ai/IAIService.hpp"
#include <string>
#include <vector>
#include <memory>

namespace ai
{

/**
 * @brief Type of LLM service provider
 */
enum class LLMProvider
{
    Ollama,      // Ollama (http://localhost:11434)
    LMStudio,    // LM Studio (http://localhost:1234)
    OpenAI,      // OpenAI API (https://api.openai.com)
    Custom       // Custom endpoint
};

/**
 * @brief Information about an installed model
 */
struct ModelInfo
{
    std::string name;
    std::string size;
    std::string modified;
};

/**
 * @brief Generic client for communicating with LLM services
 * 
 * Supports multiple providers:
 * - Ollama: Local LLM server (default: http://localhost:11434)
 * - LM Studio: Local LLM server (default: http://localhost:1234)
 * - OpenAI: Cloud API (requires API key)
 * - Custom: Any OpenAI-compatible endpoint
 */
class LLMClient : public IAIService
{
public:
    explicit LLMClient(LLMProvider provider = LLMProvider::Ollama,
                      const std::string& model = "llama3.2",
                      const std::string& baseUrl = "");
    ~LLMClient() override = default;

    std::string SendPrompt(const std::string& prompt,
        std::function<void(const std::string&)> callback = nullptr) override;

    bool IsAvailable() const override;
    std::string GetModelName() const override;

    void SetModel(const std::string& model);
    void SetBaseUrl(const std::string& url);
    void SetProvider(LLMProvider provider);
    void SetApiKey(const std::string& apiKey); // For OpenAI
    
    LLMProvider GetProvider() const { return m_provider; }
    std::string GetBaseUrl() const { return m_baseUrl; }
    
    /**
     * @brief Get list of installed models (Ollama/LM Studio only)
     * @return Vector of model information
     */
    std::vector<ModelInfo> GetInstalledModels() const;
    
    /**
     * @brief Get default base URL for a provider
     */
    static std::string GetDefaultBaseUrl(LLMProvider provider);

private:
    LLMProvider m_provider;
    std::string m_model;
    std::string m_baseUrl;
    std::string m_apiKey;

    std::string SendHttpPost(const std::string& endpoint,
                             const std::string& jsonBody) const;
    std::string SendHttpGet(const std::string& endpoint) const;
    
    std::string BuildRequestBody(const std::string& prompt) const;
    std::string ParseResponse(const std::string& response) const;
};

} // namespace ai
