#include "events_virtual_list_control.hpp"

#include <string>
#include <iostream>

namespace gui
{

  EventsVirtualListControl::EventsVirtualListControl(wxWindow *parent, const wxWindowID id, const wxPoint &pos, const wxSize &size, db::EventsContainer &events)
      : m_events(events), wxListCtrl(parent, id, pos, size, wxLC_REPORT | wxLC_VIRTUAL)
  {

    this->AppendColumn("id");
    this->AppendColumn("timestamp");
    this->AppendColumn("type");
    this->AppendColumn("info");

    m_events.RegisterOndDataUpdated(this);
  }

  void EventsVirtualListControl::OnDataUpdated()
  {
    this->SetItemCount(m_events.Size());
    this->RefreshItem(m_events.Size() - 1);
  }

  wxString EventsVirtualListControl::OnGetItemText(long index, long column) const
  {

    std::cout << "index :" << index << std::endl;

    switch (column)
    {
    case 0:
      return std::to_string(m_events.GetEvent(index).getId());
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
    this->SetItemCount(m_events.Size());
    this->Refresh();
  }
} // namespace gui
