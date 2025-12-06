#include "ui/wx/ItemVirtualListControl.hpp"

#include "ui/wx/WrappingTextRenderer.hpp"

#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/menu.h>

namespace
{
constexpr int ID_COPY_KEY = wxID_HIGHEST + 101;
constexpr int ID_COPY_BOTH = wxID_HIGHEST + 102;
}

namespace ui::wx
{

ItemVirtualListControl::ItemVirtualListControl(mvc::IModel& events,
    wxWindow* parent, const wxWindowID id, const wxPoint& pos,
    const wxSize& size)
    : wxDataViewCtrl(parent, id, pos, size,
          wxDV_ROW_LINES | wxDV_VERT_RULES | wxDV_SINGLE |
              wxDV_VARIABLE_LINE_HEIGHT)
    , m_events(events)
{
    util::Logger::Debug("ItemVirtualListControl constructed");
    m_model = std::make_unique<CustomDataModel>(m_events);
    AssociateModel(m_model.get());
    SetupColumns();

    m_events.RegisterOndDataUpdated(this);

    Bind(wxEVT_SIZE, &ItemVirtualListControl::OnColumnResized, this);
    Bind(wxEVT_CHAR_HOOK, &ItemVirtualListControl::OnKeyDown, this);
    Bind(wxEVT_CONTEXT_MENU, &ItemVirtualListControl::OnContextMenu, this);
    Bind(wxEVT_DATAVIEW_SELECTION_CHANGED, &ItemVirtualListControl::OnItemSelected, this);
    Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED, &ItemVirtualListControl::OnItemActivated, this);

    UpdateWrappingWidth();
}

ItemVirtualListControl::~ItemVirtualListControl()
{
    util::Logger::Debug("ItemVirtualListControl destroyed");
    AssociateModel(nullptr);
    m_model.reset();
}

void ItemVirtualListControl::SetupColumns()
{
    ClearColumns();

    auto* keyRenderer = new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_INERT);
    auto* keyColumn = new wxDataViewColumn("Key", keyRenderer, 0, 160, wxALIGN_LEFT,
        wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    AppendColumn(keyColumn);

    //auto* valueRenderer = new WrappingTextRenderer(400);
    auto* valueRenderer  = new wxDataViewTextRenderer("string", wxDATAVIEW_CELL_INERT);
    auto* valueColumn = new wxDataViewColumn("Value", valueRenderer, 1, 420, wxALIGN_LEFT,
        wxDATAVIEW_COL_RESIZABLE | wxDATAVIEW_COL_SORTABLE);
    AppendColumn(valueColumn);
}

void ItemVirtualListControl::RefreshAfterUpdate()
{
    if (m_model)
    {
        util::Logger::Debug("ItemVirtualListControl RefreshAfterUpdate");
        m_model->RefreshData();
        UpdateWrappingWidth();
        EnsureFirstRowSelected();
    }
}

void ItemVirtualListControl::RefreshView()
{
    RefreshAfterUpdate();
    this->Refresh();
}

void ItemVirtualListControl::ShowControl(bool show)
{
    Show(show);
}

void ItemVirtualListControl::OnDataUpdated()
{
    util::Logger::Debug("ItemVirtualListControl OnDataUpdated");
    RefreshAfterUpdate();
}

void ItemVirtualListControl::OnCurrentIndexUpdated(const int)
{
    util::Logger::Debug("ItemVirtualListControl OnCurrentIndexUpdated");
    RefreshAfterUpdate();
}

const wxString ItemVirtualListControl::getColumnName(const int column) const
{
    switch (column)
    {
        case 0:
            return wxT("Key");
        case 1:
            return wxT("Value");
        default:
            return wxT("Unknown");
    }
}

void ItemVirtualListControl::OnKeyDown(wxKeyEvent& event)
{
    if (event.GetKeyCode() == 'C' && event.ControlDown())
    {
        util::Logger::Debug("Copy shortcut detected (value/key)");
        if (event.ShiftDown())
        {
            CopyKeyToClipboard();
        }
        else
        {
            CopyValueToClipboard();
        }
        return;
    }

    if (event.GetKeyCode() == 'B' && event.ControlDown() && event.ShiftDown())
    {
        util::Logger::Debug("Copy both shortcut detected");
        CopyBothToClipboard();
        return;
    }

    event.Skip();
}

void ItemVirtualListControl::OnColumnResized(wxSizeEvent& event)
{
    UpdateWrappingWidth();
    event.Skip();
}

void ItemVirtualListControl::OnContextMenu(wxContextMenuEvent&)
{
    wxDataViewItem selection = GetSelection();
    if (!selection.IsOk())
        return;

    util::Logger::Debug("Opening ItemVirtualListControl context menu");

    wxMenu menu;
    menu.Append(wxID_COPY, wxT("Copy &Value\tCtrl+C"));
    menu.Append(ID_COPY_KEY, wxT("Copy &Key\tCtrl+Shift+C"));
    menu.Append(ID_COPY_BOTH, wxT("Copy &Both\tCtrl+Shift+B"));

    menu.Bind(wxEVT_MENU, [this](wxCommandEvent& evt) {
        switch (evt.GetId())
        {
            case wxID_COPY:
                CopyValueToClipboard();
                break;
            case ID_COPY_KEY:
                CopyKeyToClipboard();
                break;
            case ID_COPY_BOTH:
                CopyBothToClipboard();
                break;
            default:
                break;
        }
    });

    PopupMenu(&menu);
}

void ItemVirtualListControl::OnItemSelected(wxDataViewEvent& event)
{
    util::Logger::Debug("ItemVirtualListControl selection changed");
    UpdateWrappingWidth();
    event.Skip();
}

void ItemVirtualListControl::OnItemActivated(wxDataViewEvent& event)
{
    util::Logger::Debug("ItemVirtualListControl item activated");
    CopyValueToClipboard();
    event.Skip();
}

void ItemVirtualListControl::EnsureFirstRowSelected()
{
    if (!m_model)
        return;

    if (GetSelection().IsOk())
        return;

    const unsigned int rowCount = m_model->GetCount();
    if (rowCount == 0)
        return;

    wxDataViewItem firstItem = m_model->GetItem(0);
    if (!firstItem.IsOk())
        return;

    util::Logger::Trace("Auto-selecting first detail row");
    Select(firstItem);
    SetCurrentItem(firstItem);
}

void ItemVirtualListControl::CopyValueToClipboard()
{
    if (!m_model)
        return;

    wxDataViewItem selection = GetSelection();
    if (!selection.IsOk())
        return;

    unsigned int row = m_model->GetRow(selection);
    wxString value = m_model->GetOriginalValue(row);
    if (value.IsEmpty())
        return;

    util::Logger::Debug("Copying value for row {}", row);
    if (wxTheClipboard->Open())
    {
        wxTheClipboard->SetData(new wxTextDataObject(value));
        wxTheClipboard->Close();
    }
}

void ItemVirtualListControl::CopyKeyToClipboard()
{
    if (!m_model)
        return;

    wxDataViewItem selection = GetSelection();
    if (!selection.IsOk())
        return;

    unsigned int row = m_model->GetRow(selection);
    wxString key = m_model->GetOriginalKey(row);
    if (key.IsEmpty())
        return;

    util::Logger::Debug("Copying key for row {}", row);
    if (wxTheClipboard->Open())
    {
        wxTheClipboard->SetData(new wxTextDataObject(key));
        wxTheClipboard->Close();
    }
}

void ItemVirtualListControl::CopyBothToClipboard()
{
    if (!m_model)
        return;

    wxDataViewItem selection = GetSelection();
    if (!selection.IsOk())
        return;

    unsigned int row = m_model->GetRow(selection);
    wxString key = m_model->GetOriginalKey(row);
    wxString value = m_model->GetOriginalValue(row);
    if (key.IsEmpty() && value.IsEmpty())
        return;

    util::Logger::Debug("Copying key/value pair for row {}", row);
    wxString combined;
    if (!key.IsEmpty())
    {
        combined = key + wxT(": ");
    }
    combined += value;

    if (wxTheClipboard->Open())
    {
        wxTheClipboard->SetData(new wxTextDataObject(combined));
        wxTheClipboard->Close();
    }
}

void ItemVirtualListControl::UpdateWrappingWidth()
{
    if (GetColumnCount() < 2)
        return;

    wxDataViewColumn* valueColumn = GetColumn(1);
    if (!valueColumn)
        return;

    auto* renderer = dynamic_cast<WrappingTextRenderer*>(valueColumn->GetRenderer());
    if (!renderer)
        return;

    int newWidth = wxMax(50, valueColumn->GetWidth() - 8);
    if (newWidth == m_lastWrapWidth)
        return;

    m_lastWrapWidth = newWidth;
    renderer->SetMaxWidth(newWidth);
    util::Logger::Debug("Wrapping width updated to {} px", newWidth);
}

} // namespace ui::wx
