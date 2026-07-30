#pragma once

#include <string>

namespace ui
{

/**
 * @brief Toolkit-agnostic contract exposing the handful of main-window
 * interactions needed by the presenter logic.
 */
class IMainWindowView
{
  public:
    virtual ~IMainWindowView() = default;

    /** @return Current text from the search input box. */
    virtual std::string ReadSearchQuery() const = 0;

    /** @return The status-bar message currently shown to the user. */
    virtual std::string CurrentStatusText() const = 0;

    /** @brief Updates the status-bar message. */
    virtual void UpdateStatusText(const std::string& text) = 0;

    /** @brief Enables/disables the search controls to avoid re-entrancy. */
    virtual void SetSearchControlsEnabled(bool enabled) = 0;

    /** @brief Shows or hides the embedded progress indicator. */
    virtual void ToggleProgressVisibility(bool visible) = 0;

    /** @brief Adjusts the progress indicator's range. */
    virtual void ConfigureProgressRange(int range) = 0;

    /** @brief Updates the progress indicator's current value. */
    virtual void UpdateProgressValue(int value) = 0;

    /** @brief Gives the UI thread a chance to process pending events. */
    virtual void ProcessPendingEvents() = 0;

    /** @brief Requests a full window refresh/layout recalculation. */
    virtual void RefreshLayout() = 0;

    /** @brief Shows a modal input dialog.
     *  @param ok Set to true when the user accepted, false when cancelled.
     *  @return The entered string, or empty if cancelled. */
    virtual std::string AskString(const std::string& title,
        const std::string& prompt, const std::string& defaultValue,
        bool& ok) = 0;

    /** @brief Updates the filter status bar with filter information.
     *  @param totalEvents Total number of events
     *  @param filteredCount Number of events matching filters
     *  @param activeFilterCount Number of active filters
     *  @param filterDetails Description of active filters */
    virtual void UpdateFilterStatus(int totalEvents, int filteredCount,
        int activeFilterCount, const std::string& filterDetails) = 0;
};

} // namespace ui
