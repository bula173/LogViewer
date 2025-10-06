#pragma once

#include "filters/Filter.hpp"
#include <wx/button.h>
#include <wx/listctrl.h>
#include <wx/panel.h>
#include <wx/sizer.h>

namespace gui
{

class FiltersPanel : public wxPanel
{
  public:
    FiltersPanel(wxWindow* parent);

    void RefreshFilters();
    void ApplyFilters();

  private:
    void CreateControls();
    void BindEvents();

    // Event handlers
    void OnAddFilter(wxCommandEvent& event);
    void OnEditFilter(wxCommandEvent& event);
    void OnRemoveFilter(wxCommandEvent& event);
    void OnFilterSelected(wxListEvent& event);
    void OnFilterChecked(wxListEvent& event);
    void OnApplyFilters(wxCommandEvent& event);
    void OnClearFilters(wxCommandEvent& event);
    void OnListItemClick(wxMouseEvent& event);
    void OnSaveFiltersAs(wxCommandEvent& event);
    void OnLoadFilters(wxCommandEvent& event);

    // Controls
    wxListCtrl* m_filtersList = nullptr;
    wxButton* m_addButton = nullptr;
    wxButton* m_editButton = nullptr;
    wxButton* m_removeButton = nullptr;
    wxButton* m_applyButton = nullptr;
    wxButton* m_clearButton = nullptr;
    wxButton* m_saveAsButton = nullptr;
    wxButton* m_loadButton = nullptr;

    // Currently selected filter
    int m_selectedFilterIndex = -1;
};

} // namespace gui