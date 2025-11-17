#pragma once

#include "ui/IUiPanels.hpp"

#include <wx/checklst.h>
#include <wx/panel.h>

namespace ui::wx
{

class TypeFilterView : public wxPanel, public ui::ITypeFilterView
{
  public:
    TypeFilterView(wxWindow* parent, wxWindowID id = wxID_ANY,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = wxTAB_TRAVERSAL);

    void SetOnFilterChanged(std::function<void()> handler) override;
    void ReplaceTypes(
        const std::vector<std::string>& types, bool checkedByDefault) override;
    void ShowControl(bool show) override;
    void SelectAll() override;
    void DeselectAll() override;
    void InvertSelection() override;
    std::vector<std::string> CheckedTypes() const override;
    bool Empty() const override;

  private:
    void OnContextMenu(wxContextMenuEvent& event);
    void OnMenuCommand(wxCommandEvent& event);
    void OnItemToggle(wxCommandEvent& event);

    void NotifyChange();
    void ApplyToAll(const std::function<void(unsigned int)>& fn);

    wxCheckListBox* m_checkList {nullptr};
    std::function<void()> m_onChanged;

    wxDECLARE_EVENT_TABLE();
};

} // namespace ui::wx
