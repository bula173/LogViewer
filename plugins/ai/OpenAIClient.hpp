#pragma once

#include "IAIService.hpp"
#include <string>

namespace ai
{

/**
 * @brief Client for OpenAI-compatible APIs (OpenAI, xAI Grok, etc.)
 *
 * Supports any OpenAI-compatible endpoint. Pass the correct baseUrl and
 * providerName when constructing for non-OpenAI providers (e.g. xAI).
 */
class OpenAIClient : public IAIService
{
public:
    explicit OpenAIClient(const std::string& apiKey,
                         const std::string& model = "gpt-4o-mini",
                         const std::string& baseUrl = "https://api.openai.com/v1",
                         const std::string& providerName = "openai");
    ~OpenAIClient() override = default;

    std::string SendPrompt(const std::string& prompt,
        std::function<void(const std::string&)> callback = nullptr) override;

    bool IsAvailable() const override;
    std::string GetModelName() const override { return m_model; }

    void SetModelName(const std::string& model) override;

    virtual std::string GetProviderName() const override { return m_providerName; }

private:
    std::string m_apiKey;
    std::string m_model;
    std::string m_baseUrl;
    std::string m_providerName;

    std::string SendHttpPost(const std::string& endpoint,
                             const std::string& jsonBody,
                             const std::string& authHeader) const;
};

} // namespace ai
