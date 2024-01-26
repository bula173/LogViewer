#include "events_virtual_list_control.hpp"

#include <string>

namespace gui
{

  EventsVirtualListControl::EventsVirtualListControl(wxWindow *parent, const wxWindowID id, const wxPoint &pos, const wxSize &size, const db::EventsContainer &events)
      : m_events(events), wxListCtrl(parent, id, pos, size, wxLC_REPORT | wxLC_VIRTUAL)
  {

    this->AppendColumn("id");
    this->AppendColumn("timestamp");
    this->AppendColumn("type");
    this->AppendColumn("info");
    this->SetColumnWidth(0, 180);
    this->SetColumnWidth(2, 100);
  }

  wxString EventsVirtualListControl::OnGetItemText(long index, long column) const
  {
    switch (column)
    {
    case 0:
      return std::to_string(m_events[index].getId());
    case 1:
      return "dummy";
    case 2:
      return "dummy";
    case 3:
      return "dummy";
    default:
      return "";
    }
  }

  void EventsVirtualListControl::RefreshAfterUpdate()
  {
    this->SetItemCount(m_events.size());
    this->Refresh();
  }
} // namespace gui
