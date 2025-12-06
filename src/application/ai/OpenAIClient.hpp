#pragma once

#include "ai/IAIService.hpp"
#include <string>

namespace ai
{

/**
 * @brief Client for OpenAI API (ChatGPT)
 * 
 * Supports: gpt-4, gpt-4-turbo, gpt-3.5-turbo
 * API: https://api.openai.com/v1
 * Authentication: Bearer token in Authorization header
 */
class OpenAIClient : public IAIService
{
public:
    explicit OpenAIClient(const std::string& apiKey,
                         const std::string& model = "gpt-4",
                         const std::string& baseUrl = "https://api.openai.com/v1");
    ~OpenAIClient() override = default;

    std::string SendPrompt(const std::string& prompt,
        std::function<void(const std::string&)> callback = nullptr) override;

    bool IsAvailable() const override;
    std::string GetModelName() const override { return m_model; }

    void SetModel(const std::string& model) { m_model = model; }

private:
    std::string m_apiKey;
    std::string m_model;
    std::string m_baseUrl;

    std::string SendHttpPost(const std::string& endpoint,
                             const std::string& jsonBody,
                             const std::string& authHeader) const;
};

} // namespace ai
