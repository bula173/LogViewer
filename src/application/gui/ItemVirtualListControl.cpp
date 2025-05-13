#include "gui/ItemVirtualListControl.hpp"

#include <iostream>
#include <string>

namespace gui
{
ItemVirtualListControl::ItemVirtualListControl(db::EventsContainer& events,
    wxWindow* parent, const wxWindowID id, const wxPoint& pos,
    const wxSize& size)
    : m_events(events)
    , wxListCtrl(parent, id, pos, size, wxLC_REPORT | wxLC_VIRTUAL)
{

    this->AppendColumn("param");
    this->AppendColumn("value");
    // Bind resize event to auto-expand last column
    this->Bind(wxEVT_SIZE,
        [this](wxSizeEvent& evt)
        {
            // Call the default handler first
            evt.Skip();

            // Calculate total width of all columns except the last
            int totalWidth = 0;
            int colCount = this->GetColumnCount();
            for (int i = 0; i < colCount - 1; ++i)
                totalWidth += this->GetColumnWidth(i);

            // Set last column width to fill the remaining space
            int lastCol = colCount - 1;
            int clientWidth = this->GetClientSize().GetWidth();
            int newWidth = clientWidth - totalWidth - 2; // -2 for border fudge
            if (newWidth > 50)                           // minimum width
                this->SetColumnWidth(lastCol, newWidth);
        });

    m_events.RegisterOndDataUpdated(this);
}

void ItemVirtualListControl::OnDataUpdated()
{
    RefreshAfterUpdate();
}

void ItemVirtualListControl::OnCurrentIndexUpdated(const int index)
{

    auto s = m_events.GetEvent(m_events.GetCurrentItemIndex())
                 .getEventItems()
                 .size();
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

    auto items =
        m_events.GetEvent(m_events.GetCurrentItemIndex()).getEventItems();

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
    this->SetItemCount(m_events.GetEvent(m_events.GetCurrentItemIndex())
            .getEventItems()
            .size());
    this->Refresh();
    this->Update();
}
} // namespace gui
