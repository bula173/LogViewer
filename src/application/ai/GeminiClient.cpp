#include "ai/GeminiClient.hpp"
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

GeminiClient::GeminiClient(const std::string& apiKey,
                          const std::string& model,
                          const std::string& baseUrl)
    : m_apiKey(apiKey)
    , m_model(model)
    , m_baseUrl(baseUrl)
{
    util::Logger::Info("GeminiClient initialized with model: {}", m_model);
}

std::string GeminiClient::SendPrompt(const std::string& prompt,
    std::function<void(const std::string&)> callback)
{
    util::Logger::Info("Sending prompt to Gemini API (model: {})", m_model);

    // Build Gemini API request
    nlohmann::json request = {
        {"contents", {
            {
                {"parts", {
                    {{"text", prompt}}
                }}
            }
        }}
    };

    try
    {
        // Gemini uses x-goog-api-key header for authentication (not URL parameter)
        std::string endpoint;
        endpoint.reserve(20 + m_model.length()); // Pre-allocate memory
        endpoint = "/v1beta/models/";
        endpoint += m_model;
        endpoint += ":generateContent";
        
        util::Logger::Debug("Gemini endpoint: {}", endpoint);
        util::Logger::Debug("Gemini base URL: {}", m_baseUrl);
        util::Logger::Debug("Gemini full URL: {}{}", m_baseUrl, endpoint);
        util::Logger::Debug("Gemini request: {}", request.dump());
        
        const std::string response = SendHttpPost(endpoint, request.dump());
        
        util::Logger::Debug("Gemini response (first 500 chars): {}", 
                           response.substr(0, std::min(size_t(500), response.size())));
        
        // Parse Gemini response format
        nlohmann::json jsonResponse;
        try
        {
            jsonResponse = nlohmann::json::parse(response);
        }
        catch (const nlohmann::json::parse_error& e)
        {
            util::Logger::Error("Failed to parse Gemini response as JSON. Response starts with: {}",
                               response.substr(0, std::min(size_t(200), response.size())));
            return "Error: Invalid JSON response from Gemini. Response: " + 
                   response.substr(0, std::min(size_t(500), response.size()));
        }
        
        // Check for API error response
        if (jsonResponse.contains("error"))
        {
            const auto& error = jsonResponse["error"];
            std::string errorMsg = "Gemini API Error";
            if (error.contains("message"))
                errorMsg += ": " + error["message"].get<std::string>();
            if (error.contains("code"))
                errorMsg += " (code: " + std::to_string(error["code"].get<int>()) + ")";
            util::Logger::Error("{}", errorMsg);
            return "Error: " + errorMsg;
        }
        
        if (jsonResponse.contains("candidates") && !jsonResponse["candidates"].empty())
        {
            const auto& candidate = jsonResponse["candidates"][0];
            if (candidate.contains("content") && 
                candidate["content"].contains("parts") &&
                !candidate["content"]["parts"].empty())
            {
                const auto& part = candidate["content"]["parts"][0];
                if (part.contains("text"))
                {
                    return part["text"].get<std::string>();
                }
            }
        }
        
        util::Logger::Error("Invalid Gemini response format: {}", response.substr(0, 200));
        return "Error: Invalid response format from Gemini";
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("Gemini API error: {}", e.what());
        return std::string("Error: ") + e.what();
    }
}

bool GeminiClient::IsAvailable() const
{
    return !m_apiKey.empty();
}

void GeminiClient::SetModelName(const std::string& model)
{
    m_model = model;
    util::Logger::Info("Gemini model changed to: {}", m_model);
}

std::string GeminiClient::SendHttpPost(const std::string& endpoint,
                                      const std::string& jsonBody) const
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        throw std::runtime_error("Failed to initialize CURL for Gemini request");
    }

    std::string responseData;
    std::string fullUrl;
    fullUrl.reserve(m_baseUrl.length() + endpoint.length()); // Pre-allocate memory
    fullUrl = m_baseUrl;
    fullUrl += endpoint;

    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, jsonBody.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);
    
    // Use configurable timeout from config
    const int timeout = config::GetConfig().aiTimeoutSeconds;
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, static_cast<long>(timeout));

    // Set headers (Gemini uses x-goog-api-key header for authentication)
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string apiKeyHeader;
    apiKeyHeader.reserve(15 + m_apiKey.length()); // Pre-allocate memory
    apiKeyHeader = "x-goog-api-key: ";
    apiKeyHeader += m_apiKey;
    headers = curl_slist_append(headers, apiKeyHeader.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    util::Logger::Debug("Gemini POST request to: {}", fullUrl);

    const CURLcode res = curl_easy_perform(curl);
    
    long httpCode = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &httpCode);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        const std::string errorMsg = std::string("Gemini API request failed: ") + 
                                     curl_easy_strerror(res);
        util::Logger::Error("Gemini API request failed: {}", curl_easy_strerror(res));
        throw std::runtime_error(errorMsg);
    }
    
    util::Logger::Debug("Gemini response: HTTP {}, {} bytes", httpCode, responseData.size());
    
    if (httpCode >= 400)
    {
        util::Logger::Error("Gemini API returned HTTP {}: {}", httpCode, 
                           responseData.substr(0, std::min(size_t(500), responseData.size())));
        
        // Try to parse error from response
        try
        {
            auto errorJson = nlohmann::json::parse(responseData);
            if (errorJson.contains("error") && errorJson["error"].contains("message"))
            {
                throw std::runtime_error("Gemini API error: " + 
                    errorJson["error"]["message"].get<std::string>());
            }
        }
        catch (const nlohmann::json::parse_error&)
        {
            // If we can't parse, just use the raw response
        }
        
        throw std::runtime_error("Gemini API HTTP " + std::to_string(httpCode) + 
                               ": " + responseData.substr(0, std::min(size_t(200), responseData.size())));
    }

    return responseData;
}

} // namespace ai
