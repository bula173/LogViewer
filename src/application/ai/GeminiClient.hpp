#pragma once

#include "ai/IAIService.hpp"
#include <string>

namespace ai
{

/**
 * @brief Client for Google Gemini API
 * 
 * Supports: gemini-pro, gemini-1.5-pro, gemini-1.5-flash
 * API: https://generativelanguage.googleapis.com/v1
 * Authentication: API key in URL parameter
 */
class GeminiClient : public IAIService
{
public:
    explicit GeminiClient(const std::string& apiKey,
                         const std::string& model = "gemini-pro",
                         const std::string& baseUrl = "https://generativelanguage.googleapis.com/v1");
    ~GeminiClient() override = default;

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
                             const std::string& jsonBody) const;
};

} // namespace ai
