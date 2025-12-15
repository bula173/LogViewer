/**
 * @file IAIPlugin.hpp
 * @brief Interface for AI provider plugins offering services and UI panels
 */

#pragma once

#include <memory>
#include <nlohmann/json.hpp>

namespace db { class EventsContainer; }
namespace ai { class IAIService; }

namespace plugin
{

class IAIPlugin
{
public:
    virtual ~IAIPlugin() = default;

    /**
     * @brief Create an AI service instance using provided settings.
     * @param settings Arbitrary JSON settings (e.g., base URL, model, timeouts).
     * @return Unique pointer to the service or nullptr on failure.
     */
    virtual std::shared_ptr<ai::IAIService> CreateService(const nlohmann::json& settings) = 0;

    /**
     * @brief Provide the events container to the AI plugin for context.
     * @param eventsContainer Shared pointer to the events container (non-owning alias ok).
     */
    virtual void SetEventsContainer(std::shared_ptr<db::EventsContainer> eventsContainer) = 0;

    /**
     * @brief Notify plugin that events were updated (reload, filters, etc.).
     */
    virtual void OnEventsUpdated() = 0;
};

} // namespace plugin
