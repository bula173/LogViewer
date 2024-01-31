#include "events_virtual_list_control.hpp"

#include <string>
#include <iostream>

namespace gui
{
  EventsVirtualListControl::EventsVirtualListControl(db::EventsContainer &events, wxWindow *parent,
                                                     const wxWindowID id, const wxPoint &pos, const wxSize &size)
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

  const wxString EventsVirtualListControl::getColumnName(const int column) const
  {
    wxListItem item;
    item.SetMask(wxLIST_MASK_TEXT);
    this->GetColumn(column, item);
    return item.GetText();
  }

  wxString EventsVirtualListControl::OnGetItemText(long index, long column) const
  {

    std::cout << "index :" << index << std::endl;

    switch (column)
    {
    case 0:
      return std::to_string(m_events.GetEvent(index).getId());
    default:
      auto event = m_events.GetEvent(index).getEventItems();
      auto it = event.find(getColumnName(column).ToStdString());
      if (it != event.end()) {
        return it->second;
      } else {
        return "";
      }
    }
  }

  void EventsVirtualListControl::RefreshAfterUpdate()
  {
    this->SetItemCount(m_events.Size());
    this->Refresh();
  }
} // namespace gui
