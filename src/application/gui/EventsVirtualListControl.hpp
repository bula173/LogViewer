#ifndef GUI_EVENTSVIRTUALLISTCONTROL_HPP
#define GUI_EVENTSVIRTUALLISTCONTROL_HPP

#include <wx/dataview.h>
#include <wx/wx.h>

#include "gui/EventsContainerAdapter.hpp"
#include "mvc/IView.hpp"

namespace gui
{
class EventsVirtualListControl : public wxDataViewCtrl, public mvc::IView
{
  public:
    EventsVirtualListControl(db::EventsContainer& events, wxWindow* parent,
        const wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize);

    void RefreshAfterUpdate();

    // implement IView interface
    virtual void OnDataUpdated() override;
    virtual void OnCurrentIndexUpdated(const int index) override;

  private:
    db::EventsContainer& m_events;
    EventsContainerAdapter* m_model;
};

} // namespace gui

#endif // GUI_EVENTSVIRTUALLISTCONTROL_HPP
