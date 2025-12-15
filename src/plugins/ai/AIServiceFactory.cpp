#include "plugins/ai/AIServiceFactory.hpp"
#include "plugins/ai/OllamaClient.hpp"
#include "plugins/ai/OpenAIClient.hpp"
#include "plugins/ai/AnthropicClient.hpp"
#include "plugins/ai/GeminiClient.hpp"
#include "util/Logger.hpp"

namespace ai
{

std::shared_ptr<IAIService> AIServiceFactory::CreateClient(
    const std::string& provider,
    const std::string& apiKey,
    const std::string& baseUrl,
    const std::string& model)
{
    util::Logger::Info("Creating AI client for provider: {}", provider);

    if (provider == "ollama" || provider == "lmstudio" || provider == "custom")
    {
        // Local or custom OpenAI-compatible endpoints
        return std::make_shared<OllamaClient>(model, baseUrl);
    }
    else if (provider == "openai" || provider == "xai")
    {
        // OpenAI and xAI use the same API format
        // Note: API key can be empty during configuration; will be validated when sending requests
        if (apiKey.empty())
        {
            util::Logger::Warn("API key not configured for provider: {}", provider);
        }
        // OpenAI has a fixed base URL
        const std::string openaiBaseUrl = "https://api.openai.com/v1";
        return std::make_shared<OpenAIClient>(apiKey, model, openaiBaseUrl);
    }
    else if (provider == "anthropic")
    {
        // Note: API key can be empty during configuration; will be validated when sending requests
        if (apiKey.empty())
        {
            util::Logger::Warn("API key not configured for Anthropic");
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
            util::Logger::Warn("API key not configured for Google");
        }
        // Gemini has a fixed base URL
        const std::string geminiBaseUrl = "https://generativelanguage.googleapis.com";
        return std::make_shared<GeminiClient>(apiKey, model, geminiBaseUrl);
    }
    else
    {
        util::Logger::Error("Unknown AI provider: {}", provider);
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
        return "gpt-3.5-turbo";
    else if (provider == "anthropic")
        return "claude-3-sonnet-20240229";
    else if (provider == "google")
        return "gemini-2.5-flash";  // Free tier, fast and good quality
    else if (provider == "xai")
        return "grok-beta";
    else
        return "qwen2.5-coder:7b";
}

} // namespace ai
