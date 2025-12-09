#pragma once

#include "ai/IAIService.hpp"
#include <string>

namespace ai
{

/**
 * @brief Client for Anthropic API (Claude)
 * 
 * Supports: claude-3-opus, claude-3-sonnet, claude-3-haiku
 * API: https://api.anthropic.com/v1
 * Authentication: x-api-key header
 */
class AnthropicClient : public IAIService
{
public:
    explicit AnthropicClient(const std::string& apiKey,
                            const std::string& model = "claude-3-sonnet-20240229",
                            const std::string& baseUrl = "https://api.anthropic.com/v1");
    ~AnthropicClient() override = default;

    std::string SendPrompt(const std::string& prompt,
        std::function<void(const std::string&)> callback = nullptr) override;

    bool IsAvailable() const override;
    std::string GetModelName() const override { return m_model; }

    void SetModelName(const std::string& model) override;

    /**
     * @brief Get the name of the AI provider
     */
    virtual std::string GetProviderName() const override { return "anthropic"; }

private:
    std::string m_apiKey;
    std::string m_model;
    std::string m_baseUrl;

    std::string SendHttpPost(const std::string& endpoint,
                             const std::string& jsonBody,
                             const std::string& apiKeyHeader) const;
};

} // namespace ai
