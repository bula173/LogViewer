#include "ui/wx/TypeFilterView.hpp"

#include "util/WxWidgetsUtils.hpp"

#include <wx/menu.h>

namespace
{
constexpr int ID_CONTEXT_SELECT_ALL = wxID_HIGHEST + 300;
constexpr int ID_CONTEXT_DESELECT_ALL = wxID_HIGHEST + 301;
constexpr int ID_CONTEXT_INVERT = wxID_HIGHEST + 302;
}

namespace ui::wx
{

wxBEGIN_EVENT_TABLE(TypeFilterView, wxPanel)
    EVT_CONTEXT_MENU(TypeFilterView::OnContextMenu)
wxEND_EVENT_TABLE()

TypeFilterView::TypeFilterView(wxWindow* parent, wxWindowID id,
    const wxPoint& pos, const wxSize& size, long style)
    : wxPanel(parent, id, pos, size, style)
{
    auto* sizer = new wxBoxSizer(wxVERTICAL);
    m_checkList = new wxCheckListBox(this, wxID_ANY);
    sizer->Add(m_checkList, 1, wxEXPAND | wxALL, 5);
    SetSizerAndFit(sizer);

    m_checkList->Bind(wxEVT_CHECKLISTBOX, &TypeFilterView::OnItemToggle, this);
    m_checkList->Bind(wxEVT_CONTEXT_MENU, &TypeFilterView::OnContextMenu, this);
}

void TypeFilterView::SetOnFilterChanged(std::function<void()> handler)
{
    m_onChanged = std::move(handler);
}

void TypeFilterView::ReplaceTypes(
    const std::vector<std::string>& types, bool checkedByDefault)
{
    m_checkList->Clear();
    for (const auto& type : types)
    {
        const int idx = m_checkList->Append(type);
        if (checkedByDefault)
        {
            m_checkList->Check(idx, true);
        }
    }
    NotifyChange();
}

void TypeFilterView::ShowControl(bool show)
{
    Show(show);
}

void TypeFilterView::SelectAll()
{
    ApplyToAll([this](unsigned int i) { m_checkList->Check(i, true); });
    NotifyChange();
}

void TypeFilterView::DeselectAll()
{
    ApplyToAll([this](unsigned int i) { m_checkList->Check(i, false); });
    NotifyChange();
}

void TypeFilterView::InvertSelection()
{
    ApplyToAll([this](unsigned int i) {
        m_checkList->Check(i, !m_checkList->IsChecked(i));
    });
    NotifyChange();
}

std::vector<std::string> TypeFilterView::CheckedTypes() const
{
    std::vector<std::string> types;
    types.reserve(m_checkList->GetCount());

    wxArrayInt checkedIndices;
    m_checkList->GetCheckedItems(checkedIndices);
    for (auto idx : checkedIndices)
    {
        types.push_back(m_checkList->GetString(wx_utils::int_to_uint(idx)).ToStdString());
    }

    return types;
}

bool TypeFilterView::Empty() const
{
    return m_checkList->GetCount() == 0;
}

void TypeFilterView::OnContextMenu(wxContextMenuEvent& event)
{
    wxMenu menu;
    menu.Append(ID_CONTEXT_SELECT_ALL, wxT("Select All"));
    menu.Append(ID_CONTEXT_DESELECT_ALL, wxT("Deselect All"));
    menu.Append(ID_CONTEXT_INVERT, wxT("Invert Selection"));

    menu.Bind(wxEVT_MENU, &TypeFilterView::OnMenuCommand, this);
    PopupMenu(&menu);
    event.Skip();
}

void TypeFilterView::OnMenuCommand(wxCommandEvent& event)
{
    switch (event.GetId())
    {
        case ID_CONTEXT_SELECT_ALL:
            SelectAll();
            break;
        case ID_CONTEXT_DESELECT_ALL:
            DeselectAll();
            break;
        case ID_CONTEXT_INVERT:
            InvertSelection();
            break;
        default:
            event.Skip();
            break;
    }
}

void TypeFilterView::OnItemToggle(wxCommandEvent& event)
{
    wxUnusedVar(event);
    NotifyChange();
}

void TypeFilterView::NotifyChange()
{
    if (m_onChanged)
    {
        m_onChanged();
    }
}

void TypeFilterView::ApplyToAll(const std::function<void(unsigned int)>& fn)
{
    const unsigned int count = m_checkList->GetCount();
    for (unsigned int i = 0; i < count; ++i)
    {
        fn(i);
    }
}

} // namespace ui::wx
