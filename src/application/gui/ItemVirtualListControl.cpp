#include "gui/ItemVirtualListControl.hpp"
#include "util/WxWidgetsUtils.hpp"
#include "util/Logger.hpp"
#include <spdlog/spdlog.h>

#include <iostream>
#include <string>
#include <wx/clipbrd.h>
#include <wx/dataview.h>
#include <wx/dcclient.h>

namespace gui
{

ItemVirtualListControl::ItemVirtualListControl(db::EventsContainer& events,
    wxWindow* parent, const wxWindowID id, const wxPoint& pos,
    const wxSize& size)
    : wxDataViewListCtrl(
          parent, id, pos, size, wxDV_ROW_LINES | wxDV_VERT_RULES)
    , m_events(events)
{
    util::Logger::Debug("ItemVirtualListControl::ItemVirtualListControl constructed");
    // Define columns
    this->AppendTextColumn("Key", wxDATAVIEW_CELL_INERT, 150, wxALIGN_LEFT,
        wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    this->AppendTextColumn("Value", wxDATAVIEW_CELL_INERT, 300, wxALIGN_LEFT,
        wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    // Bind size event
    this->Bind(wxEVT_SIZE, &ItemVirtualListControl::OnColumnResized, this);

    // Use tooltips for long text
    this->Bind(wxEVT_MOTION, &ItemVirtualListControl::OnMouseMove, this);

    m_events.RegisterOndDataUpdated(this);

    // Bind right-click or Ctrl+C for copying value
    this->Bind(wxEVT_COMMAND_MENU_SELECTED,
        &ItemVirtualListControl::OnCopyValue, this, wxID_COPY);
}


void ItemVirtualListControl::OnCopyValue(wxCommandEvent& WXUNUSED(event))
{
    int selectedRow = this->GetSelectedRow();
    if (selectedRow == wxNOT_FOUND)
        return;
    wxVariant value;
    this->GetValue(value, wx_utils::int_to_uint(selectedRow), 1); // column 1 is "value"
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

void ItemVirtualListControl::OnCurrentIndexUpdated(const int index)
{
    try
    {
        util::Logger::Debug("ItemVirtualListControl::OnCurrentIndexUpdated called "
                      "with index: {}",
            index);

        if (index < 0 || index >= static_cast<int>(m_events.Size()))
        {
            util::Logger::Warn(
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
        util::Logger::Error("Exception in OnCurrentIndexUpdated: {}", ex.what());
        this->DeleteAllItems();
    }
    catch (...)
    {
        util::Logger::Error("Unknown exception in OnCurrentIndexUpdated");
        this->DeleteAllItems();
    }
}

void ItemVirtualListControl::RefreshAfterUpdate()
{
    try
    {
        util::Logger::Debug("ItemVirtualListControl::RefreshAfterUpdate called");

        int currentIndex = m_events.GetCurrentItemIndex();

        if (currentIndex < 0 ||
            currentIndex >= static_cast<int>(m_events.Size()))
        {
            util::Logger::Warn(
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
        util::Logger::Error("Exception in RefreshAfterUpdate: {}", ex.what());
        this->DeleteAllItems();
    }
    catch (...)
    {
        util::Logger::Error("Unknown exception in RefreshAfterUpdate");
        this->DeleteAllItems();
    }
}

void ItemVirtualListControl::OnMouseMove(wxMouseEvent& event)
{
    wxPoint pos = event.GetPosition();
    wxDataViewItem item;
    wxDataViewColumn* column;

    // Hit test to see which item is under the mouse
    this->HitTest(pos, item, column);

    if (item.IsOk() && column)
    {
        // Use a simpler approach that works across platforms
        // Store current selection
        wxDataViewItemArray oldSelection;
        this->GetSelections(oldSelection);

        // Select the item under mouse temporarily
        this->UnselectAll();
        this->Select(item);

        // Get selected row
        int row = this->GetSelectedRow();
        int col = this->GetColumnPosition(column);

        // Restore original selection
        this->UnselectAll();
        for (const auto& selItem : oldSelection)
        {
            this->Select(selItem);
        }

        if (row != wxNOT_FOUND && col != wxNOT_FOUND)
        {
            // Get the data value
            wxVariant value;
            this->GetValue(value, wx_utils::int_to_uint(row), wx_utils::int_to_uint(col));
            wxString text = value.GetString();

            // Use column width to determine if tooltip is needed
            wxClientDC dc(this);
            wxSize textExtent = dc.GetTextExtent(text);
            int colWidth = column->GetWidth();

            if (textExtent.GetWidth() > colWidth || text.Contains("\n"))
            {
                SetToolTip(text);
            }
            else
            {
                UnsetToolTip();
            }
            return;
        }
    }

    UnsetToolTip();
    event.Skip();
}

void ItemVirtualListControl::OnItemHover(wxDataViewEvent& event)
{
    // This is needed for tooltip detection
    // We don't actually want to start a drag, so veto it
    event.Veto();
}

// Simplified handler for column resize
void ItemVirtualListControl::OnColumnResized(wxSizeEvent& event)
{
    // Just refresh the control
    this->Refresh();
    event.Skip();
}

void ItemVirtualListControl::OnDataUpdated()
{
    // Update data
    RefreshAfterUpdate();
}

} // namespace gui
