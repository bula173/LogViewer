#include "ai/AnthropicClient.hpp"
#include "config/Config.hpp"
#include "util/Logger.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>

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

AnthropicClient::AnthropicClient(const std::string& apiKey,
                                 const std::string& model,
                                 const std::string& baseUrl)
    : m_apiKey(apiKey)
    , m_model(model)
    , m_baseUrl(baseUrl)
{
    util::Logger::Info("AnthropicClient initialized with model: {}", m_model);
}

std::string AnthropicClient::SendPrompt(const std::string& prompt,
    std::function<void(const std::string&)> callback)
{
    util::Logger::Info("Sending prompt to Anthropic API");

    // Build Anthropic API request (different format than OpenAI)
    nlohmann::json request = {
        {"model", m_model},
        {"messages", {
            {
                {"role", "user"},
                {"content", prompt}
            }
        }},
        {"max_tokens", 4096}
    };

    try
    {
        const std::string response = SendHttpPost("/messages", 
                                                   request.dump(), 
                                                   m_apiKey);
        
        // Parse Anthropic response format
        auto jsonResponse = nlohmann::json::parse(response);
        
        if (jsonResponse.contains("content") && !jsonResponse["content"].empty())
        {
            const auto& content = jsonResponse["content"][0];
            if (content.contains("text"))
            {
                return content["text"].get<std::string>();
            }
        }
        
        util::Logger::Error("Invalid Anthropic response format");
        return "Error: Invalid response from Anthropic";
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("Anthropic API error: {}", e.what());
        return std::string("Error: ") + e.what();
    }
}

bool AnthropicClient::IsAvailable() const
{
    return !m_apiKey.empty();
}

void AnthropicClient::SetModelName(const std::string& model)
{
    m_model = model;
    util::Logger::Info("Anthropic model changed to: {}", m_model);
}

std::string AnthropicClient::SendHttpPost(const std::string& endpoint,
                                          const std::string& jsonBody,
                                          const std::string& apiKeyHeader) const
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        throw std::runtime_error("Failed to initialize CURL for Anthropic request");
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

    // Set headers (Anthropic requires x-api-key and anthropic-version headers)
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    const std::string apiKey = "x-api-key: " + apiKeyHeader;
    headers = curl_slist_append(headers, apiKey.c_str());
    headers = curl_slist_append(headers, "anthropic-version: 2023-06-01");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    util::Logger::Debug("Anthropic POST request to: {}", fullUrl);

    const CURLcode res = curl_easy_perform(curl);
    
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        const std::string errorMsg = std::string("Anthropic API request failed: ") + 
                                     curl_easy_strerror(res);
        util::Logger::Error("Anthropic API request failed: {}", curl_easy_strerror(res));
        throw std::runtime_error(errorMsg);
    }
    
    util::Logger::Debug("Anthropic response: HTTP {}, {} bytes", httpCode, responseData.size());
    
    if (httpCode >= 400)
    {
        util::Logger::Error("Anthropic API returned HTTP {}: {}", httpCode, 
                           responseData.substr(0, std::min(size_t(500), responseData.size())));
        
        // Try to parse error from response
        try
        {
            auto errorJson = nlohmann::json::parse(responseData);
            if (errorJson.contains("error") && errorJson["error"].contains("message"))
            {
                throw std::runtime_error("Anthropic API error: " + 
                    errorJson["error"]["message"].get<std::string>());
            }
        }
        catch (const nlohmann::json::parse_error&)
        {
            // If we can't parse, just use the raw response
        }
        
        throw std::runtime_error("Anthropic API HTTP " + std::to_string(httpCode) + 
                               ": " + responseData.substr(0, std::min(size_t(200), responseData.size())));
    }

    return responseData;
}

} // namespace ai
