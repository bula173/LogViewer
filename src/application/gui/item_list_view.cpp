#include "gui/item_list_view.hpp"

#include <string>
#include <iostream>

namespace gui
{
  ItemVirtualListControl::ItemVirtualListControl(db::EventsContainer &events, wxWindow *parent,
                                                 const wxWindowID id, const wxPoint &pos, const wxSize &size)
      : m_events(events), wxListCtrl(parent, id, pos, size, wxLC_REPORT | wxLC_VIRTUAL)
  {

    this->AppendColumn("param");
    this->AppendColumn("value");

    m_events.RegisterOndDataUpdated(this);
  }

  void ItemVirtualListControl::OnDataUpdated()
  {
    RefreshAfterUpdate();
  }

  void ItemVirtualListControl::OnCurrentIndexUpdated(const int index)
  {

    auto s = m_events.GetEvent(m_events.GetCurrentItemIndex()).getEventItems().size();
    this->SetItemCount(s);
    this->RefreshItem(s - 1);
    this->Update();
  }

  const wxString ItemVirtualListControl::getColumnName(const int column) const
  {
    wxListItem item;
    item.SetMask(wxLIST_MASK_TEXT);
    this->GetColumn(column, item);
    return item.GetText();
  }

  wxString ItemVirtualListControl::OnGetItemText(long index, long column) const
  {

    auto items = m_events.GetEvent(m_events.GetCurrentItemIndex()).getEventItems();

    switch (column)
    {
    case 0:
      return items[index].first;
    case 1:
      return items.at(index).second;
    default:
      return "";
    }
  }

  void ItemVirtualListControl::RefreshAfterUpdate()
  {
    this->SetItemCount(m_events.GetEvent(m_events.GetCurrentItemIndex()).getEventItems().size());
    this->Refresh();
    this->Update();
  }
} // namespace gui
