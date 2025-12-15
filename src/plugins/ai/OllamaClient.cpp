#include "plugins/ai/OllamaClient.hpp"
#include "config/Config.hpp"
#include "util/Logger.hpp"

#include <sstream>
#include <iomanip>
#include <stdexcept>
#include <curl/curl.h>

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

// Escape string for JSON
std::string EscapeJson(const std::string& input)
{
    std::ostringstream escaped;
    for (char c : input)
    {
        switch (c)
        {
            case '"':  escaped << "\\\""; break;
            case '\\': escaped << "\\\\"; break;
            case '\b': escaped << "\\b"; break;
            case '\f': escaped << "\\f"; break;
            case '\n': escaped << "\\n"; break;
            case '\r': escaped << "\\r"; break;
            case '\t': escaped << "\\t"; break;
            default:
                if (static_cast<unsigned char>(c) < 0x20)
                {
                    escaped << "\\u" 
                            << std::hex << std::setw(4) << std::setfill('0') 
                            << static_cast<int>(c);
                }
                else
                {
                    escaped << c;
                }
        }
    }
    return escaped.str();
}
} // anonymous namespace

OllamaClient::OllamaClient(const std::string& model, const std::string& baseUrl)
    : m_model(model)
    , m_baseUrl(baseUrl)
{
    util::Logger::Info("OllamaClient initialized with model: {}, baseUrl: {}",
        m_model, m_baseUrl);
}

std::string OllamaClient::SendPrompt(const std::string& prompt,
    std::function<void(const std::string&)> callback)
{
    util::Logger::Debug("Sending prompt to Ollama (length: {})", prompt.length());

    // Build JSON request with properly escaped prompt
    const std::string escapedPrompt = EscapeJson(prompt);
    std::ostringstream jsonBody;
    jsonBody << "{"
             << R"("model":")" << m_model << R"(",)"
             << R"("prompt":")" << escapedPrompt << R"(",)"
             << R"("stream":false)"
             << "}";

    try
    {
        const std::string response = SendHttpPost("/api/generate", jsonBody.str());
        
        util::Logger::Debug("Raw Ollama response: {}", response.substr(0, 200));
        
        // Check for error response
        if (response.find(R"("error":")") != std::string::npos)
        {
            // Extract error message
            const std::string errorKey = R"("error":")";
            const size_t errorPos = response.find(errorKey);
            if (errorPos != std::string::npos)
            {
                const size_t errorStart = errorPos + errorKey.length();
                const size_t errorEnd = response.find('"', errorStart);
                const std::string errorMsg = response.substr(errorStart, errorEnd - errorStart);
                
                util::Logger::Error("Ollama error: {}", errorMsg);
                
                // Check for specific error types
                if (errorMsg.find("not found") != std::string::npos)
                {
                    return "Error: Model '" + m_model + "' not found.\n\n"
                           "Please download it first:\n"
                           "  ollama pull " + m_model + "\n\n"
                           "Or select a different model from the dropdown.";
                }
                
                return "Error from Ollama: " + errorMsg;
            }
        }
        
        // Parse response JSON to extract the "response" field
        // Simple extraction - look for "response":"..." pattern
        const std::string searchKey = R"("response":")";
        const size_t startPos = response.find(searchKey);
        if (startPos == std::string::npos)
        {
            util::Logger::Error("Could not find 'response' field in Ollama output. Response: {}", 
                response.substr(0, 500));
            return "Error: Invalid response format from Ollama. Please check if Ollama is running correctly.";
        }

        size_t contentStart = startPos + searchKey.length();
        size_t contentEnd = contentStart;
        
        // Find the closing quote, handling escaped quotes
        while (contentEnd < response.length())
        {
            if (response[contentEnd] == '"' && 
                (contentEnd == 0 || response[contentEnd - 1] != '\\'))
            {
                break;
            }
            ++contentEnd;
        }

        if (contentEnd >= response.length())
        {
            util::Logger::Error("Malformed JSON response from Ollama");
            return "Error: Malformed response from Ollama";
        }

        std::string result = response.substr(contentStart, contentEnd - contentStart);
        
        // Unescape newlines and quotes
        size_t pos = 0;
        while ((pos = result.find("\\n", pos)) != std::string::npos)
        {
            result.replace(pos, 2, "\n");
            ++pos;
        }
        pos = 0;
        while ((pos = result.find("\\\"", pos)) != std::string::npos)
        {
            result.replace(pos, 2, "\"");
            ++pos;
        }

        if (callback)
            callback(result);

        util::Logger::Debug("Received response from Ollama (length: {})", result.length());
        return result;
    }
    catch (const std::exception& e)
    {
        const std::string errorMsg = std::string("Ollama request failed: ") + e.what();
        util::Logger::Error("{}", errorMsg);
        return "Error: " + errorMsg;
    }
}

bool OllamaClient::IsAvailable() const
{
    try
    {
        const std::string response = SendHttpPost("/api/tags", "{}");
        return !response.empty();
    }
    catch (...)
    {
        return false;
    }
}

std::string OllamaClient::GetModelName() const
{
    return m_model;
}

void OllamaClient::SetModelName(const std::string& model)
{
    m_model = model;
    util::Logger::Info("Ollama model changed to: {}", m_model);
}

void OllamaClient::SetBaseUrl(const std::string& url)
{
    m_baseUrl = url;
    util::Logger::Info("Ollama base URL changed to: {}", m_baseUrl);
}

std::vector<ModelInfo> OllamaClient::GetInstalledModels() const
{
    std::vector<ModelInfo> models;
    
    try
    {
        const std::string response = SendHttpGet("/api/tags");
        
        util::Logger::Debug("GetInstalledModels raw response: {}", response.substr(0, 500));
        
        // Parse JSON response to extract models array
        // Look for "models":[...] pattern
        const std::string modelsKey = R"("models":[)";
        const size_t modelsPos = response.find(modelsKey);
        
        if (modelsPos == std::string::npos)
        {
            util::Logger::Warn("Could not find 'models' array in /api/tags response. Full response: {}", 
                response.substr(0, 500));
            return models;
        }
        
        // Simple parsing: find each "name": field
        size_t pos = modelsPos;
        while (true)
        {
            const std::string nameKey = R"("name":")";
            pos = response.find(nameKey, pos);
            if (pos == std::string::npos)
                break;
                
            const size_t nameStart = pos + nameKey.length();
            const size_t nameEnd = response.find('"', nameStart);
            if (nameEnd == std::string::npos)
                break;
                
            ModelInfo info;
            info.name = response.substr(nameStart, nameEnd - nameStart);
            
            util::Logger::Debug("Found model: {}", info.name);
            
            // Try to extract size (optional)
            const std::string sizeKey = R"("size":)";
            const size_t sizePos = response.find(sizeKey, nameEnd);
            if (sizePos != std::string::npos && sizePos < response.find(nameKey, nameEnd))
            {
                const size_t sizeStart = sizePos + sizeKey.length();
                const size_t sizeEnd = response.find_first_of(",}", sizeStart);
                if (sizeEnd != std::string::npos)
                {
                    const long long sizeBytes = std::stoll(response.substr(sizeStart, sizeEnd - sizeStart));
                    // Convert to human-readable format
                    if (sizeBytes < 1024 * 1024)
                        info.size = std::to_string(sizeBytes / 1024) + " KB";
                    else if (sizeBytes < 1024 * 1024 * 1024)
                        info.size = std::to_string(sizeBytes / (1024 * 1024)) + " MB";
                    else
                        info.size = std::to_string(sizeBytes / (1024 * 1024 * 1024)) + " GB";
                }
            }
            
            models.push_back(info);
            pos = nameEnd;
        }
        
        util::Logger::Info("Found {} installed Ollama models", models.size());
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("Failed to get installed models: {}", e.what());
    }
    
    return models;
}

std::string OllamaClient::SendHttpPost(const std::string& endpoint,
                                        const std::string& jsonBody) const
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        throw std::runtime_error("Failed to initialize CURL");
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

    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    const CURLcode res = curl_easy_perform(curl);
    
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        throw std::runtime_error(std::string("CURL request failed: ") + 
                                 curl_easy_strerror(res));
    }

    return responseData;
}

std::string OllamaClient::SendHttpGet(const std::string& endpoint) const
{
    CURL* curl = curl_easy_init();
    if (!curl)
    {
        throw std::runtime_error("Failed to initialize CURL");
    }

    std::string responseData;
    const std::string fullUrl = m_baseUrl + endpoint;

    curl_easy_setopt(curl, CURLOPT_URL, fullUrl.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPGET, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseData);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 10L);

    const CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK)
    {
        throw std::runtime_error(std::string("CURL GET request failed: ") + 
                                 curl_easy_strerror(res));
    }

    return responseData;
}

} // namespace ai
