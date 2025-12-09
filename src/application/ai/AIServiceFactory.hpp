#pragma once

#include "ai/IAIService.hpp"
#include <memory>
#include <string>

namespace ai
{

    class AIServiceWrapper {
    public:
        std::shared_ptr<IAIService> service;
    };

/**
 * @brief Factory for creating AI service clients based on provider
 * 
 * Supported providers:
 * - ollama: Local Ollama server
 * - lmstudio: Local LM Studio server
 * - openai: OpenAI/ChatGPT API
 * - anthropic: Anthropic/Claude API
 * - google: Google/Gemini API
 * - xai: xAI/Grok API
 */
class AIServiceFactory
{
public:
    /**
     * @brief Create an AI service client based on provider configuration
     * 
     * @param provider Provider name (ollama, lmstudio, openai, etc.)
     * @param apiKey API key for cloud providers (empty for local)
     * @param baseUrl Base URL for the API endpoint
     * @param model Model name to use
     * @return Shared pointer to IAIService implementation
     */
    static std::shared_ptr<IAIService> CreateClient(
        const std::string& provider,
        const std::string& apiKey,
        const std::string& baseUrl,
        const std::string& model);

    /**
     * @brief Check if a provider requires an API key
     * @param provider Provider name
     * @return true if provider requires API key
     */
    static bool RequiresApiKey(const std::string& provider);

    /**
     * @brief Get default base URL for a provider
     * @param provider Provider name
     * @return Default base URL
     */
    static std::string GetDefaultBaseUrl(const std::string& provider);

    /**
     * @brief Get default model for a provider
     * @param provider Provider name
     * @return Default model name
     */
    static std::string GetDefaultModel(const std::string& provider);
};

} // namespace ai
