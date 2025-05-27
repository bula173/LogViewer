#include "gui/ItemVirtualListControl.hpp"
#include <spdlog/spdlog.h>

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
    spdlog::debug(
        "ItemVirtualListControl::OnCurrentIndexUpdated called with index: {}",
        index);

    if (index < 0 || index > static_cast<int>(m_events.Size()))
    {
        spdlog::error(
            "ItemVirtualListControl::OnCurrentIndexUpdated: index out "
            "of range");
        return;
    }

    if (index > 0)
    {
        auto s = m_events.GetEvent(m_events.GetCurrentItemIndex())
                     .getEventItems()
                     .size();
        spdlog::debug(
            "ItemVirtualListControl::Current event item count: {}", s);

        this->SetItemCount(s);
        this->RefreshItem(s - 1);
    }
    else
    {

        this->SetItemCount(0);
        this->RefreshItem(0);
    }

    this->Update();
}

const wxString ItemVirtualListControl::getColumnName(const int column) const
{
    spdlog::debug(
        "ItemVirtualListControl::getColumnName called for column: {}", column);

    wxListItem item;
    item.SetMask(wxLIST_MASK_TEXT);
    this->GetColumn(column, item);
    return item.GetText();
}

wxString ItemVirtualListControl::OnGetItemText(long index, long column) const
{
    spdlog::debug("ItemVirtualListControl::OnGetItemText called for index: {}, "
                  "column: {}",
        index, column);

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
    spdlog::debug("ItemVirtualListControl::RefreshAfterUpdate called");

    if (m_events.GetCurrentItemIndex() < 0)
    {
        spdlog::error(
            "ItemVirtualListControl::RefreshAfterUpdate: index out of range");
        return;
    }
    if (m_events.GetCurrentItemIndex() > static_cast<int>(m_events.Size()))
    {
        spdlog::error(
            "ItemVirtualListControl::RefreshAfterUpdate: index out of range");
        return;
    }

    if (m_events.GetCurrentItemIndex() == 0)
    {

        this->SetItemCount(0);
    }
    else
    {


        this->SetItemCount(m_events.GetEvent(m_events.GetCurrentItemIndex())
                .getEventItems()
                .size());
    }

    this->Refresh();
    this->Update();
}
} // namespace gui
