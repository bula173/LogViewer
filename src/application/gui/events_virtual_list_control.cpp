#include "gui/events_virtual_list_control.hpp"

#include <string>

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
    this->AppendColumn("dummy");

    m_events.RegisterOndDataUpdated(this);

    this->Bind(wxEVT_LIST_ITEM_SELECTED, [this](wxListEvent &evt)
               { this->m_events.SetCurrentItem(evt.GetIndex()); });
  }

  void EventsVirtualListControl::OnDataUpdated()
  {

    auto s = m_events.Size();

    this->SetItemCount(s);
    this->RefreshItem(s - 1);
    this->Update();
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
    switch (column)
    {
    case 0:
      return std::to_string(m_events.GetEvent(index).getId());
    default:
      return m_events.GetEvent(index).findByKey(getColumnName(column).ToStdString());
    }
  }

  void EventsVirtualListControl::RefreshAfterUpdate()
  {
    this->SetItemCount(m_events.Size());
    this->Refresh();
    this->Update();
  }

  void EventsVirtualListControl::OnCurrentIndexUpdated(const int index)
  {
    // do nothing
  }
} // namespace gui
