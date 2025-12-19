/**
 * @file IAnalysisPlugin.hpp
 * @brief Analysis plugin interface for accessing and analyzing log events
 * @author LogViewer Development Team
 * @date 2025
 */

#ifndef PLUGIN_IANALYSISPLUGIN_HPP
#define PLUGIN_IANALYSISPLUGIN_HPP

#include "EventsContainer.hpp"
#include <memory>

namespace plugin
{

/**
 * @class IAnalysisPlugin
 * @brief Interface for plugins that need access to log events for analysis
 * 
 * This interface provides plugins with read-only access to the events container
 * for performing custom analysis, metrics calculation, and visualization.
 */
class IAnalysisPlugin
{
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IAnalysisPlugin() = default;

    /**
     * @brief Sets the events container for analysis
     * @param eventsContainer Shared pointer to the events container
     * 
     * This method is called by the application when events are loaded.
     * Plugins should store this reference and use it for analysis.
     */
    virtual void SetEventsContainer(std::shared_ptr<db::EventsContainer> eventsContainer) = 0;

    /**
     * @brief Called when events are updated
     * 
     * This method is called whenever the events container is modified
     * (e.g., new events loaded, filters applied). Plugins should refresh
     * their analysis and update their UI.
     */
    virtual void OnEventsUpdated() = 0;

    /**
     * @brief Gets the name of the analysis
     * @return Human-readable name for the analysis tab
     */
    virtual std::string GetAnalysisName() const = 0;
};

} // namespace plugin

#endif // PLUGIN_IANALYSISPLUGIN_HPP
