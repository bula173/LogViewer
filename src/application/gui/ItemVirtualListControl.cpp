#include "gui/ItemVirtualListControl.hpp"
#include "util/WxWidgetsUtils.hpp"
#include "util/Logger.hpp"
#include <spdlog/spdlog.h>

#include <iostream>
#include <string>
#include <wx/clipbrd.h>
#include <wx/dcclient.h>

namespace gui
{

ItemVirtualListControl::ItemVirtualListControl(db::EventsContainer& events,
    wxWindow* parent, const wxWindowID id, const wxPoint& pos,
    const wxSize& size)
    : wxListCtrl(parent, id, pos, size, 
          wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_HRULES | wxLC_VRULES)
    , m_events(events)
{
    util::Logger::Debug("ItemVirtualListControl::ItemVirtualListControl constructed");
    
    // Create columns with word wrapping support
    wxListItem col0;
    col0.SetId(0);
    col0.SetText("Key");
    col0.SetWidth(150);
    this->InsertColumn(0, col0);
    
    wxListItem col1;
    col1.SetId(1);
    col1.SetText("Value");
    col1.SetWidth(wxLIST_AUTOSIZE_USEHEADER);
    this->InsertColumn(1, col1);
    
    // Bind size event to resize value column
    this->Bind(wxEVT_SIZE, &ItemVirtualListControl::OnColumnResized, this);

    m_events.RegisterOndDataUpdated(this);

    // Bind Ctrl+C for copying value
    this->Bind(wxEVT_CHAR_HOOK, &ItemVirtualListControl::OnKeyDown, this);
    
    // Bind right-click context menu for copying
    this->Bind(wxEVT_CONTEXT_MENU, &ItemVirtualListControl::OnContextMenu, this);
}


void ItemVirtualListControl::OnContextMenu(wxContextMenuEvent& event)
{
    wxMenu menu;
    
    long selectedRow = this->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (selectedRow != -1)
    {
        menu.Append(wxID_COPY, "Copy Value\tCtrl+C");
        menu.AppendSeparator();
        menu.Append(wxID_ANY, "Copy Key", "Copy the key name");
        menu.Append(wxID_ANY + 1, "Copy Both", "Copy key and value");
        
        menu.Bind(wxEVT_COMMAND_MENU_SELECTED, 
            [this](wxCommandEvent& evt) {
                if (evt.GetId() == wxID_COPY) {
                    CopyValueToClipboard();
                } else if (evt.GetId() == wxID_ANY) {
                    CopyKeyToClipboard();
                } else if (evt.GetId() == wxID_ANY + 1) {
                    CopyBothToClipboard();
                }
            });
        
        PopupMenu(&menu);
    }
    
    event.Skip();
}

void ItemVirtualListControl::CopyValueToClipboard()
{
    long selectedRow = this->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (selectedRow == -1)
        return;
    
    wxString value = this->GetItemText(selectedRow, 1); // column 1 is "value"
    
    if (wxTheClipboard->Open())
    {
        wxTheClipboard->SetData(new wxTextDataObject(value));
        wxTheClipboard->Close();
        util::Logger::Debug("Copied value to clipboard: {}", value.ToStdString());
    }
}

void ItemVirtualListControl::CopyKeyToClipboard()
{
    long selectedRow = this->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (selectedRow == -1)
        return;
    
    wxString key = this->GetItemText(selectedRow, 0); // column 0 is "key"
    
    if (wxTheClipboard->Open())
    {
        wxTheClipboard->SetData(new wxTextDataObject(key));
        wxTheClipboard->Close();
        util::Logger::Debug("Copied key to clipboard: {}", key.ToStdString());
    }
}

void ItemVirtualListControl::CopyBothToClipboard()
{
    long selectedRow = this->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (selectedRow == -1)
        return;
    
    wxString key = this->GetItemText(selectedRow, 0);
    wxString value = this->GetItemText(selectedRow, 1);
    wxString combined = key + ": " + value;
    
    if (wxTheClipboard->Open())
    {
        wxTheClipboard->SetData(new wxTextDataObject(combined));
        wxTheClipboard->Close();
        util::Logger::Debug("Copied both to clipboard: {}", combined.ToStdString());
    }
}

void ItemVirtualListControl::OnCopyValue(wxCommandEvent& WXUNUSED(event))
{
    CopyValueToClipboard();
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
        m_currentItems = items;
        
        long itemIdx = 0;
        int valueColumnWidth = this->GetColumnWidth(1);
        if (valueColumnWidth <= 0)
        {
            valueColumnWidth = this->GetClientSize().GetWidth() - 150 - 20;
        }
        
        for (const auto& item : items)
        {
            // Use FromUTF8 to properly handle Polish characters
            wxString key = wxString::FromUTF8(item.first.c_str());
            wxString value = wxString::FromUTF8(item.second.c_str());
            
            // Wrap long text to fit column width
            wxString wrappedValue = WrapText(value, valueColumnWidth);
            
            // Insert item
            long idx = this->InsertItem(itemIdx, key);
            this->SetItem(idx, 1, wrappedValue);
            itemIdx++;
        }

        // Trigger column resize after adding items
        wxSizeEvent sizeEvent(this->GetSize());
        OnColumnResized(sizeEvent);
        
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
        m_currentItems = items;
        
        long itemIdx = 0;
        int valueColumnWidth = this->GetColumnWidth(1);
        if (valueColumnWidth <= 0)
        {
            valueColumnWidth = this->GetClientSize().GetWidth() - 150 - 20;
        }
        
        for (const auto& item : items)
        {
            // Use FromUTF8 to properly handle Polish characters
            wxString key = wxString::FromUTF8(item.first.c_str());
            wxString value = wxString::FromUTF8(item.second.c_str());
            
            // Wrap long text to fit column width
            wxString wrappedValue = WrapText(value, valueColumnWidth);
            
            // Insert item
            long idx = this->InsertItem(itemIdx, key);
            this->SetItem(idx, 1, wrappedValue);
            itemIdx++;
        }

        // Trigger column resize after adding items
        wxSizeEvent sizeEvent(this->GetSize());
        OnColumnResized(sizeEvent);
        
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

// Simplified handler for column resize
void ItemVirtualListControl::OnColumnResized(wxSizeEvent& event)
{
    // Resize value column to take all remaining space
    int totalWidth = this->GetClientSize().GetWidth();
    int keyWidth = this->GetColumnWidth(0);
    int valueWidth = totalWidth - keyWidth - 4; // 4 pixels for borders/scrollbar
    
    if (valueWidth > 50) // Minimum width
    {
        this->SetColumnWidth(1, valueWidth);
    }
    
    this->Refresh();
    event.Skip();
}

void ItemVirtualListControl::OnDataUpdated()
{
    // Update data
    RefreshAfterUpdate();
}

wxString ItemVirtualListControl::WrapText(const wxString& text, int width)
{
    if (width <= 50 || text.IsEmpty() || text.length() < 50)
        return text;
    
    wxString result;
    int pos = 0;
    int lineLength = width / 8; // Approximate character width
    
    while (pos < (int)text.length())
    {
        int endPos = pos + lineLength;
        
        if (endPos >= (int)text.length())
        {
            // Last piece
            result += text.Mid(pos);
            break;
        }
        
        // Find last space before the limit
        int spacePos = text.rfind(' ', endPos);
        if (spacePos > pos && spacePos - pos < lineLength * 1.5)
        {
            endPos = spacePos;
        }
        
        result += text.Mid(pos, endPos - pos);
        if (endPos < (int)text.length())
        {
            result += "\n";
        }
        
        pos = endPos + (text[endPos] == ' ' ? 1 : 0);
    }
    
    return result;
}

} // namespace gui
