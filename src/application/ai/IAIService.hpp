#pragma once

#include <string>
#include <functional>

namespace ai
{

/**
 * @brief Interface for AI service communication
 */
class IAIService
{
public:
    virtual ~IAIService() = default;

    /**
     * @brief Send a prompt to the AI and get a response
     * @param prompt The text prompt to send
     * @param callback Function called with response chunks as they arrive
     * @return Complete response text
     */
    virtual std::string SendPrompt(const std::string& prompt,
        std::function<void(const std::string&)> callback = nullptr) = 0;

    /**
     * @brief Check if the AI service is available
     * @return true if service can be reached
     */
    virtual bool IsAvailable() const = 0;

    /**
     * @brief Get the name of the model being used
     */
    virtual std::string GetModelName() const = 0;

    virtual void SetModelName(const std::string& modelName) = 0;

    /**
     * @brief Get the name of the AI provider
     */
    virtual std::string GetProviderName() const = 0;
};

} // namespace ai
