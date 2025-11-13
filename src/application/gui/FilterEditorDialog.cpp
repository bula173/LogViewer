#include "FilterEditorDialog.hpp"
#include "config/Config.hpp"
#include "filters/FilterManager.hpp"
#include "util/WxWidgetsUtils.hpp"
#include <spdlog/spdlog.h>
#include <wx/msgdlg.h>
#include <wx/notebook.h>
#include <wx/sizer.h>
#include <wx/spinctrl.h>
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

    // Filter type choice
    wxBoxSizer* typeBoxSizer = new wxBoxSizer(wxHORIZONTAL);
    typeBoxSizer->Add(new wxStaticText(this, wxID_ANY, "Filter Type:"), 0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);

    m_filterTypeChoice = new wxChoice(this, wxID_ANY);
    m_filterTypeChoice->Append("Column Filter");
    m_filterTypeChoice->Append("Parameter Filter");
    typeBoxSizer->Add(m_filterTypeChoice, 1);
    m_filterTypeChoice->SetSelection(0);

    mainSizer->Add(typeBoxSizer, 0, wxEXPAND | wxALL, 10);

    // Create tabbed interface for different filter types
    m_notebook = new wxNotebook(this, wxID_ANY);

    // Column filter panel
    wxPanel* columnPanel = new wxPanel(m_notebook);
    CreateColumnFilterControls(columnPanel);

    // Parameter filter panel
    wxPanel* paramPanel = new wxPanel(m_notebook);
    CreateParameterFilterControls(paramPanel);

    m_notebook->AddPage(columnPanel, "Column Filter");
    m_notebook->AddPage(paramPanel, "Parameter Filter");

    mainSizer->Add(m_notebook, 1, wxEXPAND | wxALL, 10);

    // Common controls
    wxFlexGridSizer* commonGrid = new wxFlexGridSizer(2, 5, 10);
    commonGrid->AddGrowableCol(1);

    // Name
    commonGrid->Add(
        new wxStaticText(this, wxID_ANY, "Name:"), 0, wxALIGN_CENTER_VERTICAL);
    m_nameCtrl = new wxTextCtrl(this, wxID_ANY);
    commonGrid->Add(m_nameCtrl, 1, wxEXPAND);

    // Pattern
    commonGrid->Add(new wxStaticText(this, wxID_ANY, "Pattern (regex):"), 0,
        wxALIGN_CENTER_VERTICAL);
    m_patternCtrl = new wxTextCtrl(this, wxID_ANY);
    commonGrid->Add(m_patternCtrl, 1, wxEXPAND);

    // Options
    commonGrid->Add(new wxStaticText(this, wxID_ANY, "Options:"), 0,
        wxALIGN_CENTER_VERTICAL);
    wxBoxSizer* optionsSizer = new wxBoxSizer(wxHORIZONTAL);
    m_caseSensitiveCheckbox = new wxCheckBox(this, wxID_ANY, "Case sensitive");
    m_invertedCheckbox = new wxCheckBox(this, wxID_ANY, "Inverted match");
    optionsSizer->Add(m_caseSensitiveCheckbox, 0, wxRIGHT, 10);
    optionsSizer->Add(m_invertedCheckbox, 0);
    commonGrid->Add(optionsSizer, 1, wxEXPAND);

    mainSizer->Add(commonGrid, 0, wxEXPAND | wxALL, 10);

    // Buttons
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(
        new wxStaticText(this, wxID_ANY, ""), 1, wxEXPAND); // Spacer
    wxButton* okButton = new wxButton(this, wxID_OK, "OK");
    wxButton* cancelButton = new wxButton(this, wxID_CANCEL, "Cancel");
    buttonSizer->Add(okButton, 0, wxRIGHT, 5);
    buttonSizer->Add(cancelButton, 0);

    mainSizer->Add(buttonSizer, 0, wxEXPAND | wxALL, 10);

    // Bind events
    m_filterTypeChoice->Bind(
        wxEVT_CHOICE, &FilterEditorDialog::OnFilterTypeChanged, this);

    // If editing existing filter, populate fields and select appropriate type
    if (m_filter)
    {
        m_nameCtrl->SetValue(m_filter->name);
        m_patternCtrl->SetValue(m_filter->pattern);
        m_caseSensitiveCheckbox->SetValue(m_filter->isCaseSensitive);
        m_invertedCheckbox->SetValue(m_filter->isInverted);

        if (m_filter->isParameterFilter)
        {
            m_filterTypeChoice->SetSelection(1);
            m_notebook->SetSelection(1);

            // Set parameter filter specific fields
            m_parameterKeyCtrl->SetValue(m_filter->parameterKey);
            m_depthSpinner->SetValue(m_filter->parameterDepth);
        }
        else
        {
            m_filterTypeChoice->SetSelection(0);
            m_notebook->SetSelection(0);

            // Set column filter specific fields
            int colIndex = m_columnChoice->FindString(m_filter->columnName);
            if (colIndex != wxNOT_FOUND)
            {
                m_columnChoice->SetSelection(colIndex);
            }
        }
    }

    SetSizer(mainSizer);
}

void FilterEditorDialog::CreateColumnFilterControls(wxPanel* parent)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxFlexGridSizer* grid = new wxFlexGridSizer(2, 5, 10);
    grid->AddGrowableCol(1);

    // Column
    grid->Add(new wxStaticText(parent, wxID_ANY, "Column:"), 0,
        wxALIGN_CENTER_VERTICAL);
    m_columnChoice = new wxChoice(parent, wxID_ANY);
    PopulateColumnChoices();
    grid->Add(m_columnChoice, 1, wxEXPAND);

    sizer->Add(grid, 0, wxEXPAND | wxALL, 10);

    // Description
    sizer->Add(new wxStaticText(parent, wxID_ANY,
                   "Column filters match against a specific column value."),
        0, wxALL, 10);

    parent->SetSizer(sizer);
}

void FilterEditorDialog::CreateParameterFilterControls(wxPanel* parent)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);

    wxFlexGridSizer* grid = new wxFlexGridSizer(2, 5, 10);
    grid->AddGrowableCol(1);

    // Parameter key
    grid->Add(new wxStaticText(parent, wxID_ANY, "Parameter Key:"), 0,
        wxALIGN_CENTER_VERTICAL);
    m_parameterKeyCtrl = new wxTextCtrl(parent, wxID_ANY);
    grid->Add(m_parameterKeyCtrl, 1, wxEXPAND);

    // Search depth
    grid->Add(new wxStaticText(parent, wxID_ANY, "Search Depth:"), 0,
        wxALIGN_CENTER_VERTICAL);
    m_depthSpinner = new wxSpinCtrl(parent, wxID_ANY, "0", wxDefaultPosition,
        wxDefaultSize, wxSP_ARROW_KEYS, 0, 5, 0);
    grid->Add(m_depthSpinner, 0);

    sizer->Add(grid, 0, wxEXPAND | wxALL, 10);

    // Description
    sizer->Add(new wxStaticText(parent, wxID_ANY,
                   "Parameter filters search for a specific parameter key in "
                   "the event data.\n"
                   "Search depth controls how deep to look in nested "
                   "structures (0 = top level only)."),
        0, wxALL, 10);

    parent->SetSizer(sizer);
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

void FilterEditorDialog::OnFilterTypeChanged(wxCommandEvent& /*event*/)
{
    // Change the notebook page when the filter type changes
    int selection = m_filterTypeChoice->GetSelection();
    m_notebook->SetSelection(static_cast<size_t>(selection));
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

    // Get pattern
    wxString pattern = m_patternCtrl->GetValue().Trim();
    if (pattern.IsEmpty())
    {
        wxMessageBox("Pattern cannot be empty.", "Validation Error",
            wxOK | wxICON_EXCLAMATION);
        m_patternCtrl->SetFocus();
        return;
    }

    // Determine filter type
    bool isParameterFilter = (m_filterTypeChoice->GetSelection() == 1);

    // Get type-specific values
    wxString columnName;
    wxString paramKey;
    int depth = 0;

    if (isParameterFilter)
    {
        paramKey = m_parameterKeyCtrl->GetValue().Trim();
        if (paramKey.IsEmpty())
        {
            wxMessageBox("Parameter key cannot be empty.", "Validation Error",
                wxOK | wxICON_EXCLAMATION);
            m_parameterKeyCtrl->SetFocus();
            return;
        }
        depth = m_depthSpinner->GetValue();
    }
    else
    {
        int colIdx = m_columnChoice->GetSelection();
        if (colIdx == wxNOT_FOUND)
        {
            wxMessageBox("Please select a column.", "Validation Error",
                wxOK | wxICON_EXCLAMATION);
            m_columnChoice->SetFocus();
            return;
        }
        columnName = m_columnChoice->GetString(wx_utils::int_to_uint(colIdx));
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
            m_caseSensitiveCheckbox->GetValue(), m_invertedCheckbox->GetValue(),
            isParameterFilter, paramKey.ToStdString(), depth);
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
        m_filter->pattern = pattern.ToStdString();
        m_filter->isCaseSensitive = m_caseSensitiveCheckbox->GetValue();
        m_filter->isInverted = m_invertedCheckbox->GetValue();
        m_filter->isParameterFilter = isParameterFilter;

        if (isParameterFilter)
        {
            m_filter->parameterKey = paramKey.ToStdString();
            m_filter->parameterDepth = depth;
            m_filter->columnName =
                "*"; // Use wildcard column for parameter filters
        }
        else
        {
            m_filter->columnName = columnName.ToStdString();
            m_filter->parameterKey = "";
            m_filter->parameterDepth = 0;
        }

        m_filter->compile(); // Recompile regex
    }

    EndModal(wxID_OK);
}

void FilterEditorDialog::OnCancel(wxCommandEvent& WXUNUSED(event))
{
    EndModal(wxID_CANCEL);
}


} // namespace gui