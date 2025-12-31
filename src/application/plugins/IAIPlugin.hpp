// Interface for AI provider plugins (internal C++ interface)
#pragma once

#include <memory>
#include <nlohmann/json.hpp>

namespace db { class EventsContainer; }
namespace ai { class IAIService; }

namespace plugin {

class IAIPlugin
{
public:
    virtual ~IAIPlugin() = default;
    virtual std::shared_ptr<ai::IAIService> CreateService(const nlohmann::json& settings) = 0;
    virtual void SetEventsContainer(std::shared_ptr<db::EventsContainer> eventsContainer) = 0;
    virtual void OnEventsUpdated() = 0;
};

} // namespace plugin
