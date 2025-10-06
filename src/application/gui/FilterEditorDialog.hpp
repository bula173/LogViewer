#pragma once

#include "filters/Filter.hpp"
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/dialog.h>
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

    // Filter being edited (nullptr for new filter)
    filters::FilterPtr m_filter;
};

} // namespace gui