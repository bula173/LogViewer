#ifndef GUI_EVENTSVIRTUALLISTCONTROL_HPP
#define GUI_EVENTSVIRTUALLISTCONTROL_HPP

#include <wx/dataview.h>
#include <wx/wx.h>

#include "mvc/IModel.hpp"
#include "mvc/IView.hpp"
#include "ui/IEventsView.hpp"
#include "ui/wx/EventsContainerAdapter.hpp"

namespace ui::wx
{
class EventsVirtualListControl : public wxDataViewCtrl,
                                 public mvc::IView,
                                 public ui::IEventsListView
{
  public:
    EventsVirtualListControl(mvc::IModel& events, wxWindow* parent,
        const wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize);

    void RefreshAfterUpdate();

    /**
     * @brief Sets the filtered events to be displayed in the control.
     * @param filteredIndices Vector of indices (or IDs) of events to display.
     */
    void SetFilteredEvents(
      const std::vector<unsigned long>& filteredIndices) override;

    /**
     * @brief Update colors and refresh the view.
     *
     * Call this after configuration changes to update row colors.
     */
    void UpdateColors() override;

    /**
     * @brief Refresh the columns based on current configuration.
     * Call this after configuration changes to update columns.
     * @note This does not refresh the data, just the columns.
     */
    void RefreshColumns() override;

    /** @brief Implements IEventsListView::RefreshView. */
    void RefreshView() override;

    // implement IView interface
    virtual void OnDataUpdated() override;
    virtual void OnCurrentIndexUpdated(const int index) override;

  private:
    mvc::IModel& m_events;
    EventsContainerAdapter* m_model;

    // Helper to count expected visible columns
    unsigned int CountVisibleConfigColumns() const;

    // Helper to add columns from configuration
    void AddColumnsFromConfig();
};

} // namespace ui::wx

#endif // GUI_EVENTSVIRTUALLISTCONTROL_HPP
