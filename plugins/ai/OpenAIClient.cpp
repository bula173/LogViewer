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
    // Reject API keys that contain CR or LF — they would inject extra HTTP
    // headers into the Authorization field (HTTP header injection).
    if (m_apiKey.find_first_of("\r\n") != std::string::npos)
    {
        PLUGIN_LOG(PLUGIN_LOG_ERROR,
            "OpenAIClient: API key contains illegal whitespace characters — rejected");
        m_apiKey.clear(); // treat as missing; SendPrompt / IsAvailable handle empty
    }
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
    // Return true iff an API key is configured — do NOT make a live network
    // probe here.  A blocking curl_easy_perform() call from this method (which
    // can be invoked on the UI thread or from a background thread) creates the
    // pattern:  background thread + outbound TCP connection that looks like a
    // C2 beacon to Windows Defender's heuristic engine.  The actual API
    // availability is validated implicitly when the first real prompt is sent.
    if (m_apiKey.empty())
    {
        PLUGIN_LOG(PLUGIN_LOG_WARN, "OpenAI API key is not configured");
        return false;
    }
    PLUGIN_LOG(PLUGIN_LOG_DEBUG, "OpenAI API key is configured (len={})", m_apiKey.size());
    return true;
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
    // RAII wrappers so both the CURL handle and the header list are freed on
    // every exit path (normal return, exception, or early throw).
    struct CurlDeleter  { void operator()(CURL*       p) const { if (p) curl_easy_cleanup(p);  } };
    struct SlistDeleter { void operator()(curl_slist* p) const { if (p) curl_slist_free_all(p); } };

    std::unique_ptr<CURL,       CurlDeleter > curl(curl_easy_init());
    if (!curl)
        throw std::runtime_error("Failed to initialize CURL for OpenAI request");

    std::string responseData;
    const std::string fullUrl = m_baseUrl + endpoint;

    curl_easy_setopt(curl.get(), CURLOPT_URL,           fullUrl.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_POSTFIELDS,    jsonBody.c_str());
    curl_easy_setopt(curl.get(), CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl.get(), CURLOPT_WRITEDATA,     &responseData);

    // Use configurable timeout from config
    const int timeout = config::GetConfig().aiTimeoutSeconds;
    curl_easy_setopt(curl.get(), CURLOPT_TIMEOUT, static_cast<long>(timeout));

    // Set headers with Bearer token authentication
    std::unique_ptr<curl_slist, SlistDeleter> headers(
        curl_slist_append(nullptr, "Content-Type: application/json"));
    headers.reset(curl_slist_append(headers.release(), authHeader.c_str()));
    curl_easy_setopt(curl.get(), CURLOPT_HTTPHEADER, headers.get());

    PLUGIN_LOG(PLUGIN_LOG_DEBUG, "OpenAI POST request to: {}", fullUrl);

    const CURLcode res = curl_easy_perform(curl.get());

    long httpCode = 0;
    curl_easy_getinfo(curl.get(), CURLINFO_RESPONSE_CODE, &httpCode);
    // RAII destructors free headers and curl handle here automatically.

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
