#include "gui/ItemVirtualListControl.hpp"
#include <spdlog/spdlog.h>

#include <iostream>
#include <string>
#include <wx/clipbrd.h>

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

    // Bind right-click or Ctrl+C for copying value
    this->Bind(wxEVT_COMMAND_MENU_SELECTED,
        &ItemVirtualListControl::OnCopyValue, this, wxID_COPY);
    this->Bind(wxEVT_DATAVIEW_ITEM_CONTEXT_MENU,
        &ItemVirtualListControl::OnContextMenu, this);
    this->Bind(wxEVT_KEY_DOWN, &ItemVirtualListControl::OnKeyDown, this);
}

void ItemVirtualListControl::OnContextMenu(wxDataViewEvent& WXUNUSED(event))
{
    wxMenu menu;
    menu.Append(wxID_COPY, "Copy Value");
    PopupMenu(&menu);
}

void ItemVirtualListControl::OnCopyValue(wxCommandEvent& WXUNUSED(event))
{
    int selectedRow = this->GetSelectedRow();
    if (selectedRow == wxNOT_FOUND)
        return;
    wxVariant value;
    this->GetValue(value, selectedRow, 1); // column 1 is "value"
    if (wxTheClipboard->Open())
    {
        wxTheClipboard->SetData(new wxTextDataObject(value.GetString()));
        wxTheClipboard->Close();
    }
}

void ItemVirtualListControl::OnKeyDown(wxKeyEvent& event)
{
    if (event.GetKeyCode() == 'C' && event.ControlDown())
    {
        wxCommandEvent eventCopy(wxEVT_COMMAND_MENU_SELECTED, wxID_COPY);
        OnCopyValue(eventCopy);
        return;
    }
    event.Skip();
}

void ItemVirtualListControl::OnDataUpdated()
{
    RefreshAfterUpdate();
}

void ItemVirtualListControl::OnCurrentIndexUpdated(const int index)
{
    try
    {
        spdlog::debug("ItemVirtualListControl::OnCurrentIndexUpdated called "
                      "with index: {}",
            index);

        if (index < 0 || index >= static_cast<int>(m_events.Size()))
        {
            spdlog::warn(
                "ItemVirtualListControl::OnCurrentIndexUpdated: index {} out "
                "of range (size: {})",
                index, m_events.Size());
            this->DeleteAllItems();
            return;
        }

        this->DeleteAllItems();

        auto items = m_events.GetEvent(index).getEventItems();
        for (const auto& item : items)
        {
            wxVector<wxVariant> data;
            data.push_back(wxVariant(item.first));
            data.push_back(wxVariant(item.second));
            this->AppendItem(data);
        }

        this->Update();
    }
    catch (const std::exception& ex)
    {
        spdlog::error("Exception in OnCurrentIndexUpdated: {}", ex.what());
        this->DeleteAllItems();
    }
    catch (...)
    {
        spdlog::error("Unknown exception in OnCurrentIndexUpdated");
        this->DeleteAllItems();
    }
}

void ItemVirtualListControl::RefreshAfterUpdate()
{
    try
    {
        spdlog::debug("ItemVirtualListControl::RefreshAfterUpdate called");

        int currentIndex = m_events.GetCurrentItemIndex();

        if (currentIndex < 0 ||
            currentIndex >= static_cast<int>(m_events.Size()))
        {
            spdlog::warn(
                "ItemVirtualListControl::RefreshAfterUpdate: index {} out "
                "of range (size: {})",
                currentIndex, m_events.Size());
            this->DeleteAllItems();
            return;
        }

        this->DeleteAllItems();

        auto items = m_events.GetEvent(currentIndex).getEventItems();
        for (const auto& item : items)
        {
            wxVector<wxVariant> data;
            data.push_back(wxVariant(item.first));
            data.push_back(wxVariant(item.second));
            this->AppendItem(data);
        }

        this->Refresh();
        this->Update();
    }
    catch (const std::exception& ex)
    {
        spdlog::error("Exception in RefreshAfterUpdate: {}", ex.what());
        this->DeleteAllItems();
    }
    catch (...)
    {
        spdlog::error("Unknown exception in RefreshAfterUpdate");
        this->DeleteAllItems();
    }
}
} // namespace gui
