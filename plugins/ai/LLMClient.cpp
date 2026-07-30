#include "LLMClient.hpp"
#include "PluginLoggerC.h"
#include <fmt/format.h>
#include <curl/curl.h>

#define PLUGIN_LOG(level, ...) do { std::string _pl_msg = fmt::format(__VA_ARGS__); PluginLogger_Log(level, _pl_msg.c_str()); } while(0)

namespace ai
{

namespace
{
// Callback for libcurl to write received data
size_t WriteCallback(void* contents, size_t size, size_t nmemb, std::string* userp)
{
    const size_t totalSize = size * nmemb;
    userp->append(static_cast<char*>(contents), totalSize);
    return totalSize;
}
} // anonymous namespace

LLMClient::LLMClient(LLMProvider provider,
                     const std::string& model,
                     const std::string& baseUrl)
    : m_provider(provider)
    , m_model(model)
    , m_baseUrl(baseUrl.empty() ? GetDefaultBaseUrl(provider) : baseUrl)
{
    PLUGIN_LOG(PLUGIN_LOG_INFO, "LLMClient initialized: provider={}, model={}, baseUrl={}",
               static_cast<int>(provider), m_model, m_baseUrl);
}

std::string LLMClient::SendPrompt(const std::string& prompt,
    std::function<void(const std::string&)> callback)
{
    PLUGIN_LOG(PLUGIN_LOG_DEBUG, "LLMClient::SendPrompt called");
    return "Error: LLMClient::SendPrompt not yet implemented";
}

bool LLMClient::IsAvailable() const
{
    PLUGIN_LOG(PLUGIN_LOG_DEBUG, "LLMClient::IsAvailable called");
    return true;
}

std::string LLMClient::GetModelName() const
{
    return m_model;
}

void LLMClient::SetModelName(const std::string& model)
{
    m_model = model;
    PLUGIN_LOG(PLUGIN_LOG_INFO, "LLMClient model changed to: {}", m_model);
}

std::string LLMClient::GetProviderName() const
{
    switch (m_provider)
    {
        case LLMProvider::Ollama:    return "ollama";
        case LLMProvider::LMStudio:  return "lmstudio";
        case LLMProvider::OpenAI:    return "openai";
        case LLMProvider::Custom:    return "custom";
        default:                     return "unknown";
    }
}

std::string LLMClient::GetServiceId() const
{
    return GetProviderName() + ":" + m_model;
}

void LLMClient::SetBaseUrl(const std::string& url)
{
    m_baseUrl = url;
    PLUGIN_LOG(PLUGIN_LOG_INFO, "LLMClient base URL changed to: {}", m_baseUrl);
}

void LLMClient::SetProvider(LLMProvider provider)
{
    m_provider = provider;
    if (m_baseUrl.empty() || m_baseUrl == GetDefaultBaseUrl(static_cast<LLMProvider>(static_cast<int>(m_provider) - 1)))
    {
        m_baseUrl = GetDefaultBaseUrl(provider);
    }
    PLUGIN_LOG(PLUGIN_LOG_INFO, "LLMClient provider changed to: {}", GetProviderName());
}

void LLMClient::SetApiKey(const std::string& apiKey)
{
    m_apiKey = apiKey;
    PLUGIN_LOG(PLUGIN_LOG_INFO, "LLMClient API key configured");
}

std::vector<ModelInfo> LLMClient::GetInstalledModels() const
{
    std::vector<ModelInfo> models;
    PLUGIN_LOG(PLUGIN_LOG_DEBUG, "LLMClient::GetInstalledModels - not yet implemented");
    return models;
}

std::string LLMClient::GetDefaultBaseUrl(LLMProvider provider)
{
    switch (provider)
    {
        case LLMProvider::Ollama:   return "http://localhost:11434";
        case LLMProvider::LMStudio: return "http://localhost:1234";
        case LLMProvider::OpenAI:   return "https://api.openai.com/v1";
        case LLMProvider::Custom:   return "http://localhost:8000";
        default:                    return "";
    }
}

std::string LLMClient::SendHttpPost(const std::string& endpoint,
                                    const std::string& jsonBody) const
{
    return "";
}

std::string LLMClient::SendHttpGet(const std::string& endpoint) const
{
    return "";
}

std::string LLMClient::BuildRequestBody(const std::string& prompt) const
{
    return "";
}

std::string LLMClient::ParseResponse(const std::string& response) const
{
    return "";
}

} // namespace ai
