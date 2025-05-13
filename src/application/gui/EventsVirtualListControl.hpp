#ifndef GUI_EVENTSVIRTUALLISTCONTROL_HPP
#define GUI_EVENTSVIRTUALLISTCONTROL_HPP

#include <wx/listctrl.h>
#include <wx/wx.h>

#include "db/EventsContainer.hpp"
#include "mvc/IView.hpp"

namespace gui
{
class EventsVirtualListControl : public wxListCtrl, public mvc::IView
{
  public:
    EventsVirtualListControl(db::EventsContainer& events, wxWindow* parent,
        const wxWindowID id = wxID_ANY, const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize);

    virtual wxString OnGetItemText(long index, long column) const wxOVERRIDE;
    void RefreshAfterUpdate();

    // implement IView interface
    virtual void OnDataUpdated() override;
    virtual void OnCurrentIndexUpdated(const int index) override;

  private:
    const wxString getColumnName(const int column) const;

  private:
    db::EventsContainer& m_events;
};

} // namespace gui

#endif // GUI_EVENTSVIRTUALLISTCONTROL_HPP
