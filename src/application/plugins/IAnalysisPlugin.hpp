// IAnalysisPlugin internal interface
#pragma once

#include "EventsContainer.hpp"
#include <memory>

namespace plugin {

class IAnalysisPlugin
{
public:
    virtual ~IAnalysisPlugin() = default;
    virtual void SetEventsContainer(std::shared_ptr<db::EventsContainer> eventsContainer) = 0;
    virtual void OnEventsUpdated() = 0;
    virtual std::string GetAnalysisName() const = 0;
};

} // namespace plugin
