#include "gui/ItemVirtualListControl.hpp"
#include <spdlog/spdlog.h>

#include <iostream>
#include <string>

namespace gui
{
ItemVirtualListControl::ItemVirtualListControl(db::EventsContainer& events,
    wxWindow* parent, const wxWindowID id, const wxPoint& pos,
    const wxSize& size)
    : wxDataViewListCtrl(
          parent, id, pos, size, wxDV_ROW_LINES | wxDV_VERT_RULES)
    , m_events(events)
{
    this->AppendTextColumn("param");
    this->AppendTextColumn("value");

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

    this->DeleteAllItems();

    if (index > 0)
    {
        auto items =
            m_events.GetEvent(m_events.GetCurrentItemIndex()).getEventItems();
        for (const auto& item : items)
        {
            wxVector<wxVariant> data;
            data.push_back(wxVariant(item.first));
            data.push_back(wxVariant(item.second));
            this->AppendItem(data);
        }
    }

    this->Update();
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

    this->DeleteAllItems();

    if (m_events.GetCurrentItemIndex() > 0)
    {
        auto items =
            m_events.GetEvent(m_events.GetCurrentItemIndex()).getEventItems();
        for (const auto& item : items)
        {
            wxVector<wxVariant> data;
            data.push_back(wxVariant(item.first));
            data.push_back(wxVariant(item.second));
            this->AppendItem(data);
        }
    }

    this->Refresh();
    this->Update();
}
} // namespace gui
