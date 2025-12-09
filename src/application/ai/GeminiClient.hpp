#pragma once

#include "ai/IAIService.hpp"
#include "util/Logger.hpp"
#include <string>

namespace ai
{

/**
 * @brief Client for Google Gemini API
 * 
 * Supported models (latest):
 * - gemini-2.5-flash (recommended for free tier - fast, efficient, 1M context)
 * - gemini-2.5-flash-lite (fastest, most economical for high-frequency tasks)
 * - gemini-2.5-pro (advanced reasoning, excellent for coding)
 * - gemini-3-pro (most intelligent, best multimodal understanding)
 * 
 * Legacy models:
 * - gemini-1.5-flash, gemini-1.5-pro (still supported)
 * 
 * Free tier limits:
 * - 15 requests per minute
 * - 1500 requests per day
 * 
 * API: https://generativelanguage.googleapis.com/v1beta/models/{model}:generateContent
 * Authentication: x-goog-api-key header
 * Get API key: https://aistudio.google.com/app/apikey
 */
class GeminiClient : public IAIService
{
public:
    explicit GeminiClient(const std::string& apiKey,
                         const std::string& model = "gemini-2.5-flash",
                         const std::string& baseUrl = "https://generativelanguage.googleapis.com");
    ~GeminiClient() override {util::Logger::Debug("GeminiClient destroyed");};

    std::string SendPrompt(const std::string& prompt,
        std::function<void(const std::string&)> callback = nullptr) override;

    bool IsAvailable() const override;
    std::string GetModelName() const override { return m_model; }

    void SetModelName(const std::string& model) override;

    /**
     * @brief Get the name of the AI provider
     */
    virtual std::string GetProviderName() const override { return "google"; }

private:
    std::string m_apiKey;
    std::string m_model;
    std::string m_baseUrl;

    std::string SendHttpPost(const std::string& endpoint,
                             const std::string& jsonBody) const;
};

} // namespace ai
