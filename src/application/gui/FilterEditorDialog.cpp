#include "FilterEditorDialog.hpp"
#include "config/Config.hpp"
#include "filters/FilterManager.hpp"
#include <spdlog/spdlog.h>
#include <wx/msgdlg.h>
#include <wx/sizer.h>
#include <wx/statbox.h>
#include <wx/statline.h>
#include <wx/stattext.h>

namespace gui
{

FilterEditorDialog::FilterEditorDialog(
    wxWindow* parent, filters::FilterPtr filter)
    : wxDialog(parent, wxID_ANY, filter ? "Edit Filter" : "New Filter",
          wxDefaultPosition, wxDefaultSize,
          wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_filter(filter)
{
    CreateControls();
    BindEvents();

    // Set size
    SetMinSize(wxSize(400, 300));
    SetSize(wxSize(450, 400));

    // Center on parent
    CenterOnParent();

    // Set initial focus
    m_nameCtrl->SetFocus();
}

void FilterEditorDialog::CreateControls()
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Filter properties section
    wxFlexGridSizer* gridSizer = new wxFlexGridSizer(2, 5, 10);
    gridSizer->AddGrowableCol(1);

    // Name
    gridSizer->Add(
        new wxStaticText(this, wxID_ANY, "Name:"), 0, wxALIGN_CENTER_VERTICAL);
    m_nameCtrl = new wxTextCtrl(this, wxID_ANY);
    gridSizer->Add(m_nameCtrl, 1, wxEXPAND);

    // Column
    gridSizer->Add(new wxStaticText(this, wxID_ANY, "Column:"), 0,
        wxALIGN_CENTER_VERTICAL);
    m_columnChoice = new wxChoice(this, wxID_ANY);
    PopulateColumnChoices();
    gridSizer->Add(m_columnChoice, 1, wxEXPAND);

    // Pattern
    gridSizer->Add(new wxStaticText(this, wxID_ANY, "Pattern (regex):"), 0,
        wxALIGN_CENTER_VERTICAL);
    m_patternCtrl = new wxTextCtrl(this, wxID_ANY);
    gridSizer->Add(m_patternCtrl, 1, wxEXPAND);

    // Options
    gridSizer->Add(new wxStaticText(this, wxID_ANY, "Options:"), 0,
        wxALIGN_CENTER_VERTICAL);
    wxBoxSizer* optionsSizer = new wxBoxSizer(wxHORIZONTAL);
    m_caseSensitiveCheckbox = new wxCheckBox(this, wxID_ANY, "Case sensitive");
    m_invertedCheckbox = new wxCheckBox(this, wxID_ANY, "Inverted match");
    optionsSizer->Add(m_caseSensitiveCheckbox, 0, wxRIGHT, 10);
    optionsSizer->Add(m_invertedCheckbox, 0);
    gridSizer->Add(optionsSizer, 1, wxEXPAND);

    mainSizer->Add(gridSizer, 0, wxEXPAND | wxALL, 10);

    // Separator
    mainSizer->Add(new wxStaticLine(this, wxID_ANY), 0, wxEXPAND | wxALL, 5);


    // Buttons
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(
        new wxStaticText(this, wxID_ANY, ""), 1, wxEXPAND); // Spacer
    wxButton* okButton = new wxButton(this, wxID_OK, "OK");
    wxButton* cancelButton = new wxButton(this, wxID_CANCEL, "Cancel");
    buttonSizer->Add(okButton, 0, wxRIGHT, 5);
    buttonSizer->Add(cancelButton, 0);

    mainSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, 10);

    // If editing existing filter, populate fields
    if (m_filter)
    {
        m_nameCtrl->SetValue(m_filter->name);

        // Find and select column
        int colIndex = m_columnChoice->FindString(m_filter->columnName);
        if (colIndex != wxNOT_FOUND)
        {
            m_columnChoice->SetSelection(colIndex);
        }

        m_patternCtrl->SetValue(m_filter->pattern);
        m_caseSensitiveCheckbox->SetValue(m_filter->isCaseSensitive);
        m_invertedCheckbox->SetValue(m_filter->isInverted);
    }
    else
    {
        // Default values for new filter
        m_columnChoice->SetSelection(0); // First column
        m_caseSensitiveCheckbox->SetValue(false);
        m_invertedCheckbox->SetValue(false);
    }

    SetSizer(mainSizer);
}

void FilterEditorDialog::BindEvents()
{
    Bind(wxEVT_BUTTON, &FilterEditorDialog::OnOK, this, wxID_OK);
    Bind(wxEVT_BUTTON, &FilterEditorDialog::OnCancel, this, wxID_CANCEL);
}

void FilterEditorDialog::PopulateColumnChoices()
{
    m_columnChoice->Clear();

    // Add all column names from config
    const auto& columns = config::GetConfig().columns;
    for (const auto& column : columns)
    {
        m_columnChoice->Append(column.name);
    }

    // Also add special options
    m_columnChoice->Append("*"); // Match any column
}

filters::FilterPtr FilterEditorDialog::GetFilter() const
{
    return m_filter;
}

void FilterEditorDialog::OnOK(wxCommandEvent& WXUNUSED(event))
{
    // Validate inputs
    wxString name = m_nameCtrl->GetValue().Trim();
    if (name.IsEmpty())
    {
        wxMessageBox("Filter name cannot be empty.", "Validation Error",
            wxOK | wxICON_EXCLAMATION);
        m_nameCtrl->SetFocus();
        return;
    }

    // Get column
    int colIdx = m_columnChoice->GetSelection();
    if (colIdx == wxNOT_FOUND)
    {
        wxMessageBox("Please select a column.", "Validation Error",
            wxOK | wxICON_EXCLAMATION);
        m_columnChoice->SetFocus();
        return;
    }
    wxString columnName = m_columnChoice->GetString(colIdx);

    // Get pattern
    wxString pattern = m_patternCtrl->GetValue().Trim();
    if (pattern.IsEmpty())
    {
        wxMessageBox("Pattern cannot be empty.", "Validation Error",
            wxOK | wxICON_EXCLAMATION);
        m_patternCtrl->SetFocus();
        return;
    }

    // Validate regex pattern
    try
    {
        std::regex testRegex(pattern.ToStdString(),
            m_caseSensitiveCheckbox->GetValue()
                ? std::regex::ECMAScript
                : std::regex::ECMAScript | std::regex::icase);
    }
    catch (const std::regex_error& e)
    {
        wxMessageBox(
            wxString::Format("Invalid regular expression: %s", e.what()),
            "Validation Error", wxOK | wxICON_EXCLAMATION);
        m_patternCtrl->SetFocus();
        return;
    }

    // Create or update filter
    if (!m_filter)
    {
        m_filter = std::make_shared<filters::Filter>(name.ToStdString(),
            columnName.ToStdString(), pattern.ToStdString(),
            m_caseSensitiveCheckbox->GetValue(),
            m_invertedCheckbox->GetValue());
    }
    else
    {
        // Check if renaming and there's already a filter with the new name
        if (m_filter->name != name.ToStdString())
        {
            auto existingFilter =
                filters::FilterManager::getInstance().getFilterByName(
                    name.ToStdString());
            if (existingFilter)
            {
                wxMessageBox("A filter with this name already exists.",
                    "Validation Error", wxOK | wxICON_EXCLAMATION);
                m_nameCtrl->SetFocus();
                return;
            }
        }

        // Update existing filter
        m_filter->name = name.ToStdString();
        m_filter->columnName = columnName.ToStdString();
        m_filter->pattern = pattern.ToStdString();
        m_filter->isCaseSensitive = m_caseSensitiveCheckbox->GetValue();
        m_filter->isInverted = m_invertedCheckbox->GetValue();
        m_filter->compile(); // Recompile regex
    }

    EndModal(wxID_OK);
}

void FilterEditorDialog::OnCancel(wxCommandEvent& WXUNUSED(event))
{
    EndModal(wxID_CANCEL);
}


} // namespace gui