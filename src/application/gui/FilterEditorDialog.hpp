#pragma once

#include "filters/Filter.hpp"
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/notebook.h>
#include <wx/panel.h> // Add this line for wxPanel
#include <wx/spinctrl.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

namespace gui
{

class FilterEditorDialog : public wxDialog
{
  public:
    FilterEditorDialog(wxWindow* parent, filters::FilterPtr filter = nullptr);

    filters::FilterPtr GetFilter() const;

  private:
    void CreateControls();
    void BindEvents();
    void PopulateColumnChoices();
    void OnOK(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);

    // Controls
    wxTextCtrl* m_nameCtrl = nullptr;
    wxChoice* m_columnChoice = nullptr;
    wxTextCtrl* m_patternCtrl = nullptr;
    wxCheckBox* m_caseSensitiveCheckbox = nullptr;
    wxCheckBox* m_invertedCheckbox = nullptr;
    wxNotebook* m_notebook = nullptr;
    wxChoice* m_filterTypeChoice = nullptr;
    wxTextCtrl* m_parameterKeyCtrl = nullptr;
    wxSpinCtrl* m_depthSpinner = nullptr;

    // Filter being edited (nullptr for new filter)
    filters::FilterPtr m_filter;

    void CreateColumnFilterControls(wxPanel* parent);
    void CreateParameterFilterControls(wxPanel* parent);
    void OnFilterTypeChanged(wxCommandEvent& event);
};

} // namespace gui