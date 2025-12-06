#include "ai/AnthropicClient.hpp"
#include "util/Logger.hpp"
#include <nlohmann/json.hpp>

namespace ai
{

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

std::string AnthropicClient::SendHttpPost(const std::string& endpoint,
                                          const std::string& jsonBody,
                                          const std::string& apiKeyHeader) const
{
    // TODO: Implement CURL HTTP POST with x-api-key header
    util::Logger::Info("Sending HTTP POST to Anthropic: {}", endpoint);
    
    throw std::runtime_error("Anthropic HTTP implementation pending");
}

} // namespace ai
