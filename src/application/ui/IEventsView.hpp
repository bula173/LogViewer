#pragma once

#include <vector>

namespace ui
{

/**
 * @brief Interface describing the primary events list view used by the UI.
 *
 * Implementations can be backed by different GUI toolkits while exposing a
 * small, toolkit-agnostic surface consumed by controllers and windows.
 */
class IEventsListView
{
  public:
    virtual ~IEventsListView() = default;

    /** @brief Rebuilds visible columns after configuration changes. */
    virtual void RefreshColumns() = 0;
    /** @brief Refreshes both the underlying model and the visual control. */
    virtual void RefreshView() = 0;
    /** @brief Applies filtered indices so only selected events are shown. */
    virtual void SetFilteredEvents(
        const std::vector<unsigned long>& filteredIndices) = 0;
    /** @brief Clears any active filter; all events become visible via O(1) path. */
    virtual void ClearFilter() = 0;
    /** @brief Updates row colors or other visual embellishments. */
    virtual void UpdateColors() = 0;
};

/**
 * @brief Interface describing the event details panel.
 */
class IEventDetailsView
{
  public:
    virtual ~IEventDetailsView() = default;

    /** @brief Refreshes the details view content. */
    virtual void RefreshView() = 0;
};

} // namespace ui
