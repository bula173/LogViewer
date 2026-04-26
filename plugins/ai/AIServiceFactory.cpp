#include "AIServiceFactory.hpp"
#include "OllamaClient.hpp"
#include "OpenAIClient.hpp"
#include "AnthropicClient.hpp"
#include "GeminiClient.hpp"
#include "PluginLoggerC.h"
#include <fmt/format.h>

#define PLUGIN_LOG(level, ...) do { std::string _pl_msg = fmt::format(__VA_ARGS__); PluginLogger_Log(level, _pl_msg.c_str()); } while(0)

namespace ai
{

std::shared_ptr<IAIService> AIServiceFactory::CreateClient(
    const std::string& provider,
    const std::string& apiKey,
    const std::string& baseUrl,
    const std::string& model)
{
    PLUGIN_LOG(PLUGIN_LOG_INFO, "Creating AI client for provider: {}", provider);

    if (provider == "ollama" || provider == "lmstudio" || provider == "custom")
    {
        // Local or custom OpenAI-compatible endpoints
        return std::make_shared<OllamaClient>(model, baseUrl);
    }
    else if (provider == "openai")
    {
        if (apiKey.empty())
            PLUGIN_LOG(PLUGIN_LOG_WARN, "API key not configured for provider: {}", provider);
        return std::make_shared<OpenAIClient>(apiKey, model, "https://api.openai.com/v1");
    }
    else if (provider == "xai")
    {
        // xAI uses OpenAI-compatible API at a different base URL
        if (apiKey.empty())
            PLUGIN_LOG(PLUGIN_LOG_WARN, "API key not configured for provider: {}", provider);
        return std::make_shared<OpenAIClient>(apiKey, model, "https://api.x.ai/v1", "xai");
    }
    else if (provider == "anthropic")
    {
        // Note: API key can be empty during configuration; will be validated when sending requests
        if (apiKey.empty())
        {
            PLUGIN_LOG(PLUGIN_LOG_WARN, "API key not configured for Anthropic");
        }
        // Anthropic has a fixed base URL
        const std::string anthropicBaseUrl = "https://api.anthropic.com/v1";
        return std::make_shared<AnthropicClient>(apiKey, model, anthropicBaseUrl);
    }
    else if (provider == "google")
    {
        // Note: API key can be empty during configuration; will be validated when sending requests
        if (apiKey.empty())
        {
            PLUGIN_LOG(PLUGIN_LOG_WARN, "API key not configured for Google");
        }
        // Gemini has a fixed base URL
        const std::string geminiBaseUrl = "https://generativelanguage.googleapis.com";
        return std::make_shared<GeminiClient>(apiKey, model, geminiBaseUrl);
    }
    else
    {
        PLUGIN_LOG(PLUGIN_LOG_ERROR, "Unknown AI provider: {}", provider);
        throw std::runtime_error("Unknown AI provider: " + provider);
    }
}

bool AIServiceFactory::RequiresApiKey(const std::string& provider)
{
    return provider == "openai" || 
           provider == "anthropic" || 
           provider == "google" || 
           provider == "xai";
}

std::string AIServiceFactory::GetDefaultBaseUrl(const std::string& provider)
{
    if (provider == "ollama")
        return "http://localhost:11434";
    else if (provider == "lmstudio")
        return "http://localhost:1234";
    else if (provider == "openai")
        return "https://api.openai.com/v1";
    else if (provider == "anthropic")
        return "https://api.anthropic.com/v1";
    else if (provider == "google")
        return "https://generativelanguage.googleapis.com/v1";
    else if (provider == "xai")
        return "https://api.x.ai/v1";
    else
        return "http://localhost:11434"; // Default to Ollama
}

std::string AIServiceFactory::GetDefaultModel(const std::string& provider)
{
    if (provider == "ollama" || provider == "lmstudio")
        return "qwen2.5-coder:7b";
    else if (provider == "openai")
        return "gpt-4o-mini";
    else if (provider == "anthropic")
        return "claude-3-7-sonnet-20250219";
    else if (provider == "google")
        return "gemini-2.5-flash";
    else if (provider == "xai")
        return "grok-3";
    else
        return "qwen2.5-coder:7b";
}

} // namespace ai
