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
};

} // namespace ui
