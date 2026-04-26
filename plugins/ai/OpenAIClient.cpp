#include "OpenAIClient.hpp"
#include "Config.hpp"
#include "PluginLoggerC.h"
#include <fmt/format.h>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

// Helper macro for formatted plugin logging
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

OpenAIClient::OpenAIClient(const std::string& apiKey,
                           const std::string& model,
                           const std::string& baseUrl,
                           const std::string& providerName)
    : m_apiKey(apiKey)
    , m_model(model)
    , m_baseUrl(baseUrl)
    , m_providerName(providerName)
{
    PLUGIN_LOG(PLUGIN_LOG_INFO, "OpenAIClient initialized with model: {}", m_model);
}

std::string OpenAIClient::SendPrompt(const std::string& prompt,
    [[maybe_unused]] std::function<void(const std::string&)> callback)
{
    // Validate API key before sending request
    if (m_apiKey.empty())
    {
        PLUGIN_LOG(PLUGIN_LOG_ERROR, "OpenAI API key not configured");
        return "Error: API key required for OpenAI. Please configure it in the AI Configuration panel.";
    }
    
    PLUGIN_LOG(PLUGIN_LOG_INFO, "Sending prompt to OpenAI API");

    // Build OpenAI API request
    nlohmann::json request = {
        {"model", m_model},
        {"messages", {
            {
                {"role", "user"},
                {"content", prompt}
            }
        }},
        {"temperature", 0.7}
    };

    const std::string authHeader = "Authorization: Bearer " + m_apiKey;
    
    try
    {
        const std::string response = SendHttpPost("/chat/completions", 
                                                   request.dump(), 
                                                   authHeader);
        
        PLUGIN_LOG(PLUGIN_LOG_DEBUG, "OpenAI response (first 500 chars): {}", 
               response.substr(0, std::min(size_t(500), response.size())));
        
        // Parse response
        nlohmann::json jsonResponse;
        try
        {
            jsonResponse = nlohmann::json::parse(response);
        }
        catch (const nlohmann::json::parse_error& e)
        {
            PLUGIN_LOG(PLUGIN_LOG_ERROR, "Failed to parse OpenAI response as JSON. Response starts with: {}",
                       response.substr(0, std::min(size_t(200), response.size())));
            return "Error: Invalid JSON response from OpenAI. Check API key and endpoint. Response: " + 
                   response.substr(0, std::min(size_t(500), response.size()));
        }
        
        // Check for API error response
        if (jsonResponse.contains("error"))
        {
            const auto& error = jsonResponse["error"];
            std::string errorMsg = "OpenAI API Error";
            if (error.contains("message"))
                errorMsg += ": " + error["message"].get<std::string>();
            if (error.contains("type"))
                errorMsg += " (" + error["type"].get<std::string>() + ")";
            PLUGIN_LOG(PLUGIN_LOG_ERROR, "{}", errorMsg);
            return "Error: " + errorMsg;
        }
        
        if (jsonResponse.contains("choices") && !jsonResponse["choices"].empty())
        {
            const auto& message = jsonResponse["choices"][0]["message"];
            if (message.contains("content"))
            {
                return message["content"].get<std::string>();
            }
        }
        
        PLUGIN_LOG(PLUGIN_LOG_ERROR, "Invalid OpenAI response format");
        return "Error: Invalid response from OpenAI";
    }
    catch (const std::exception& e)
    {
        PLUGIN_LOG(PLUGIN_LOG_ERROR, "OpenAI API error: {}", e.what());
        return std::string("Error: ") + e.what();
    }
}

bool OpenAIClient::IsAvailable() const
{
    if (m_apiKey.empty())
    {
        PLUGIN_LOG(PLUGIN_LOG_WARN, "OpenAI API key is not configured");
        return false;
    }
    
    try
    {
        // Try a minimal request to test the connection and API key
        const nlohmann::json request = {
            {"model", m_model},
            {"messages", nlohmann::json::array({
                {{"role", "user"}, {"content", "test"}}
            })},
            {"max_tokens", 1}
        };
        
        const std::string authHeader = "Authorization: Bearer " + m_apiKey;
        const std::string response = SendHttpPost("/chat/completions", 
                                                  request.dump(), 
                                                  authHeader);
        
        // If we get any response without exception, the API is available
        PLUGIN_LOG(PLUGIN_LOG_DEBUG, "OpenAI API health check successful");
        return !response.empty();
    }
    catch (const std::exception& e)
    {
        PLUGIN_LOG(PLUGIN_LOG_WARN, "OpenAI API health check failed: {}", e.what());
        return false;
    }
}

void OpenAIClient::SetModelName(const std::string& model)
{
    m_model = model;
    PLUGIN_LOG(PLUGIN_LOG_INFO, "OpenAI model changed to: {}", m_model);
}

std::string OpenAIClient::SendHttpPost(const std::string& endpoint,
                                       const std::string& jsonBody,
                                       const std::string& authHeader) const
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        throw std::runtime_error("Failed to initialize CURL for OpenAI request");
    }

    std::string responseData;
    const std::string fullUrl = m_baseUrl + endpoint;

    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);
    
    // Use configurable timeout from config
    const int timeout = config::GetConfig().aiTimeoutSeconds;
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeout));

    // Set headers with Bearer token authentication
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    headers = curl_slist_append(headers, authHeader.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    PLUGIN_LOG(PLUGIN_LOG_DEBUG, "OpenAI POST request to: {}", fullUrl);

    const CURLcode res = curl_easy_perform(curl);
    
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        const std::string errorMsg = std::string("OpenAI API request failed: ") + 
                                     curl_easy_strerror(res);
        PLUGIN_LOG(PLUGIN_LOG_ERROR, "OpenAI API request failed: {}", curl_easy_strerror(res));
        throw std::runtime_error(errorMsg);
    }
    
    PLUGIN_LOG(PLUGIN_LOG_DEBUG, "OpenAI response: HTTP {}, {} bytes", httpCode, responseData.size());
    
    if (httpCode >= 400)
    {
        PLUGIN_LOG(PLUGIN_LOG_ERROR, "OpenAI API returned HTTP {}: {}", httpCode, 
               responseData.substr(0, std::min(size_t(500), responseData.size())));
    }

    return responseData;
}

} // namespace ai
