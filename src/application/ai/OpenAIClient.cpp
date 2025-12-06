#include "ai/OpenAIClient.hpp"
#include "util/Logger.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace ai
{

OpenAIClient::OpenAIClient(const std::string& apiKey,
                           const std::string& model,
                           const std::string& baseUrl)
    : m_apiKey(apiKey)
    , m_model(model)
    , m_baseUrl(baseUrl)
{
    util::Logger::Info("OpenAIClient initialized with model: {}", m_model);
}

std::string OpenAIClient::SendPrompt(const std::string& prompt,
    std::function<void(const std::string&)> callback)
{
    util::Logger::Info("Sending prompt to OpenAI API");

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

    const std::string authHeader = "Bearer " + m_apiKey;
    
    try
    {
        const std::string response = SendHttpPost("/chat/completions", 
                                                   request.dump(), 
                                                   authHeader);
        
        // Parse response
        auto jsonResponse = nlohmann::json::parse(response);
        
        if (jsonResponse.contains("choices") && !jsonResponse["choices"].empty())
        {
            const auto& message = jsonResponse["choices"][0]["message"];
            if (message.contains("content"))
            {
                return message["content"].get<std::string>();
            }
        }
        
        util::Logger::Error("Invalid OpenAI response format");
        return "Error: Invalid response from OpenAI";
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("OpenAI API error: {}", e.what());
        return std::string("Error: ") + e.what();
    }
}

bool OpenAIClient::IsAvailable() const
{
    // TODO: Implement health check
    return !m_apiKey.empty();
}

std::string OpenAIClient::SendHttpPost(const std::string& endpoint,
                                       const std::string& jsonBody,
                                       const std::string& authHeader) const
{
    // TODO: Implement CURL HTTP POST with Bearer authentication
    // Similar to OllamaClient but with Authorization header
    util::Logger::Info("Sending HTTP POST to OpenAI: {}", endpoint);
    
    // Placeholder implementation
    throw std::runtime_error("OpenAI HTTP implementation pending");
}

} // namespace ai
