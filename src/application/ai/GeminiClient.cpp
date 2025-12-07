#include "ai/GeminiClient.hpp"
#include "util/Logger.hpp"
#include <nlohmann/json.hpp>

namespace ai
{

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
    util::Logger::Info("Sending prompt to Gemini API");

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
        // Gemini uses API key in URL: /v1/models/{model}:generateContent?key=API_KEY
        const std::string endpoint = "/v1/models/" + m_model + 
                                    ":generateContent?key=" + m_apiKey;
        
        const std::string response = SendHttpPost(endpoint, request.dump());
        
        // Parse Gemini response format
        auto jsonResponse = nlohmann::json::parse(response);
        
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
        
        util::Logger::Error("Invalid Gemini response format");
        return "Error: Invalid response from Gemini";
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

void GeminiClient::SetModel(const std::string& model)
{
    m_model = model;
    util::Logger::Info("Gemini model changed to: {}", m_model);
}

std::string GeminiClient::SendHttpPost(const std::string& endpoint,
                                      const std::string& jsonBody) const
{
    // TODO: Implement CURL HTTP POST (API key already in URL)
    util::Logger::Info("Sending HTTP POST to Gemini: {}", endpoint);
    
    throw std::runtime_error("Gemini HTTP implementation pending");
}

} // namespace ai
