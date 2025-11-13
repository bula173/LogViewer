#include "ConfigEditorDialog.hpp"
#include "config/Config.hpp"
#include "util/Logger.hpp"
#include <algorithm>
#include <wx/checkbox.h>
#include <wx/clrpicker.h>
#include <wx/colordlg.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/spinctrl.h>
#include <wx/statline.h>

#include <spdlog/common.h>
#include <spdlog/spdlog.h>

ConfigEditorDialog::ConfigEditorDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "Edit Configuration", wxDefaultPosition,
          wxSize(700, 600), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
{
    auto& cfg = config::GetConfig();

    // Create notebook for tabs
    wxNotebook* notebook = new wxNotebook(this, wxID_ANY);

    // General settings panel
    wxPanel* generalPanel = new wxPanel(notebook, wxID_ANY);
    wxBoxSizer* generalSizer = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer* grid = new wxFlexGridSizer(2, 10, 10);

    // Display config file path with "Open" button
    wxBoxSizer* pathSizer = new wxBoxSizer(wxHORIZONTAL);
    wxStaticText* pathLabel =
        new wxStaticText(generalPanel, wxID_ANY, "Config file:");
    wxFont italicFont = pathLabel->GetFont();
    italicFont.SetStyle(wxFONTSTYLE_ITALIC);

    // Get the path from config
    const std::string& configPath = cfg.GetConfigFilePath();
    wxStaticText* pathText =
        new wxStaticText(generalPanel, wxID_ANY, configPath);
    pathText->SetFont(italicFont);
    pathText->SetForegroundColour(wxColour(80, 80, 80)); // Dark gray text

    // Add "Open" button
    wxButton* openButton = new wxButton(generalPanel, wxID_ANY, "Open",
        wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    openButton->Bind(wxEVT_BUTTON, &ConfigEditorDialog::OnOpenConfigFile, this);

    pathSizer->Add(pathLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    pathSizer->Add(pathText, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    pathSizer->Add(openButton, 0, wxALIGN_CENTER_VERTICAL);
    generalSizer->Add(pathSizer, 0, wxEXPAND | wxALL, 10);

    // Create general settings fields
    grid->Add(new wxStaticText(generalPanel, wxID_ANY, "xmlRootElement"), 0,
        wxALIGN_CENTER_VERTICAL);
    wxTextCtrl* xmlRootCtrl =
        new wxTextCtrl(generalPanel, wxID_ANY, cfg.xmlRootElement);
    grid->Add(xmlRootCtrl, 1, wxEXPAND);
    m_inputs["xmlRootElement"] = xmlRootCtrl;

    grid->Add(new wxStaticText(generalPanel, wxID_ANY, "xmlEventElement"), 0,
        wxALIGN_CENTER_VERTICAL);
    wxTextCtrl* xmlEventCtrl =
        new wxTextCtrl(generalPanel, wxID_ANY, cfg.xmlEventElement);
    grid->Add(xmlEventCtrl, 1, wxEXPAND);
    m_inputs["xmlEventElement"] = xmlEventCtrl;

    // Special handling for log level - dropdown instead of text field
    grid->Add(new wxStaticText(generalPanel, wxID_ANY, "logLevel"), 0,
        wxALIGN_CENTER_VERTICAL);

    wxChoice* logLevelChoice = new wxChoice(generalPanel, wxID_ANY);

    // Add standard log levels
    logLevelChoice->Append("trace");
    logLevelChoice->Append("debug");
    logLevelChoice->Append("info");
    logLevelChoice->Append("warning");
    logLevelChoice->Append("error");
    logLevelChoice->Append("critical");
    logLevelChoice->Append("off");

    // Set current level
    if (!cfg.logLevel.empty())
    {
        int index = logLevelChoice->FindString(cfg.logLevel);
        if (index != wxNOT_FOUND)
            logLevelChoice->SetSelection(index);
        else
            logLevelChoice->SetSelection(2); // Default to "info"
    }
    else
    {
        logLevelChoice->SetSelection(2); // Default to "info"
    }

    grid->Add(logLevelChoice, 1, wxEXPAND);
    m_logLevelChoice = logLevelChoice; // Store in a class member

    // Bind event to apply log level changes immediately
    m_logLevelChoice->Bind(
        wxEVT_CHOICE, &ConfigEditorDialog::OnLogLevelChanged, this);

    // Add more fields as needed...

    grid->AddGrowableCol(1, 1);
    generalSizer->Add(grid, 1, wxALL | wxEXPAND, 10);
    generalPanel->SetSizer(generalSizer);

    // Columns configuration panel
    wxPanel* columnsPanel = new wxPanel(notebook);
    wxBoxSizer* columnsSizer = new wxBoxSizer(wxVERTICAL);

    // Create a scrolled window for columns (in case there are many)
    wxScrolledWindow* scrollWin = new wxScrolledWindow(
        columnsPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
    scrollWin->SetScrollRate(0, 10);
    wxBoxSizer* scrollSizer = new wxBoxSizer(wxVERTICAL);

    // Create list control for column management
    m_columnsList = new wxListCtrl(scrollWin, wxID_ANY, wxDefaultPosition,
        wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    m_columnsList->InsertColumn(0, "Visible", wxLIST_FORMAT_LEFT, 60);
    m_columnsList->InsertColumn(1, "Column Name", wxLIST_FORMAT_LEFT, 200);
    m_columnsList->InsertColumn(2, "Width", wxLIST_FORMAT_LEFT, 70);

    // Get column definitions from config
    const auto& columns = cfg.GetColumns();

    // Add column data to list
    for (size_t i = 0; i < columns.size(); ++i)
    {
        const auto& column = columns[i];

        long itemIndex =
            m_columnsList->InsertItem(i, column.isVisible ? "Yes" : "No");
        m_columnsList->SetItem(itemIndex, 1, column.name);
        m_columnsList->SetItem(itemIndex, 2, std::to_string(column.width));

        // Store original column name for lookup
        m_columnsList->SetItemData(itemIndex, i); // Store index for reference
    }

    // Add the list control to the sizer
    scrollSizer->Add(m_columnsList, 1, wxEXPAND | wxALL, 5);

    // Column navigation buttons
    wxBoxSizer* moveButtonSizer = new wxBoxSizer(wxVERTICAL);
    wxButton* moveUpBtn = new wxButton(scrollWin, wxID_ANY, "Move Up");
    wxButton* moveDownBtn = new wxButton(scrollWin, wxID_ANY, "Move Down");

    moveUpBtn->Bind(wxEVT_BUTTON, &ConfigEditorDialog::OnMoveColumnUp, this);
    moveDownBtn->Bind(
        wxEVT_BUTTON, &ConfigEditorDialog::OnMoveColumnDown, this);
    moveButtonSizer->Add(moveUpBtn, 0, wxBOTTOM, 5);
    moveButtonSizer->Add(moveDownBtn, 0);

    // Create a horizontal sizer for the list and navigation buttons
    wxBoxSizer* listAndNavSizer = new wxBoxSizer(wxHORIZONTAL);
    listAndNavSizer->Add(m_columnsList, 1, wxEXPAND | wxRIGHT, 5);
    listAndNavSizer->Add(moveButtonSizer, 0, wxALIGN_CENTER_VERTICAL);

    // Replace the existing add of m_columnsList with the combined sizer
    // scrollSizer->Add(m_columnsList, 1, wxEXPAND | wxALL, 5);
    scrollSizer->Add(listAndNavSizer, 1, wxEXPAND | wxALL, 5);

    // Column detail editor section
    wxStaticBoxSizer* detailSizer =
        new wxStaticBoxSizer(wxVERTICAL, scrollWin, "Column Details");
    wxFlexGridSizer* detailGrid = new wxFlexGridSizer(2, 5, 10);
    detailGrid->AddGrowableCol(1);

    // Column name
    detailGrid->Add(
        new wxStaticText(detailSizer->GetStaticBox(), wxID_ANY, "Name:"), 0,
        wxALIGN_CENTER_VERTICAL);
    m_columnName = new wxTextCtrl(detailSizer->GetStaticBox(), wxID_ANY);
    detailGrid->Add(m_columnName, 0, wxEXPAND);

    // Visible
    detailGrid->Add(
        new wxStaticText(detailSizer->GetStaticBox(), wxID_ANY, "Visible:"), 0,
        wxALIGN_CENTER_VERTICAL);
    m_columnVisible = new wxCheckBox(detailSizer->GetStaticBox(), wxID_ANY, "");
    detailGrid->Add(m_columnVisible, 0);

    // Width
    detailGrid->Add(
        new wxStaticText(detailSizer->GetStaticBox(), wxID_ANY, "Width:"), 0,
        wxALIGN_CENTER_VERTICAL);
    m_columnWidth = new wxSpinCtrl(detailSizer->GetStaticBox(), wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS, 20, 500, 100);
    detailGrid->Add(m_columnWidth, 0);
    detailSizer->Add(detailGrid, 1, wxEXPAND | wxALL, 5);

    // Apply button for details
    wxButton* applyBtn =
        new wxButton(detailSizer->GetStaticBox(), wxID_ANY, "Apply Changes");
    detailSizer->Add(applyBtn, 0, wxALIGN_RIGHT | wxALL, 5);

    scrollSizer->Add(detailSizer, 0, wxEXPAND | wxALL, 5);

    // Add/Remove buttons
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* addBtn = new wxButton(scrollWin, wxID_ANY, "Add Column");
    wxButton* removeBtn = new wxButton(scrollWin, wxID_ANY, "Remove Selected");
    buttonSizer->Add(addBtn, 0, wxRIGHT, 5);
    buttonSizer->Add(removeBtn, 0);

    scrollSizer->Add(buttonSizer, 0, wxALIGN_LEFT | wxALL, 5);

    scrollWin->SetSizer(scrollSizer);

    // Add the scrolled window to the column panel
    columnsSizer->Add(scrollWin, 1, wxEXPAND | wxALL, 5);
    columnsPanel->SetSizer(columnsSizer);

    // Add panels to notebook
    notebook->AddPage(generalPanel, "General");
    notebook->AddPage(columnsPanel, "Columns");

    // Colors tab panel
    wxPanel* colorsPanel = new wxPanel(notebook);
    wxBoxSizer* colorsSizer = new wxBoxSizer(wxVERTICAL);

    // Column selection dropdown
    wxBoxSizer* columnSelectionSizer = new wxBoxSizer(wxHORIZONTAL);
    columnSelectionSizer->Add(
        new wxStaticText(colorsPanel, wxID_ANY, "Column:"), 0,
        wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

    m_colorColumnChoice = new wxChoice(colorsPanel, wxID_ANY);
    // Populate with column names
    m_colorColumnChoice->Append("type");
    if (m_colorColumnChoice->GetCount() > 0)
    {
        m_colorColumnChoice->SetSelection(0);
    }

    columnSelectionSizer->Add(m_colorColumnChoice, 1);
    colorsSizer->Add(columnSelectionSizer, 0, wxEXPAND | wxALL, 10);

    // Current color mappings for the selected column
    wxStaticBoxSizer* colorMappingsSizer =
        new wxStaticBoxSizer(wxVERTICAL, colorsPanel, "Color Mappings");

    m_colorMappingsList =
        new wxListCtrl(colorMappingsSizer->GetStaticBox(), wxID_ANY,
            wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    m_colorMappingsList->InsertColumn(0, "Value", wxLIST_FORMAT_LEFT, 150);
    m_colorMappingsList->InsertColumn(
        1, "Background Color", wxLIST_FORMAT_LEFT, 120);
    m_colorMappingsList->InsertColumn(2, "Text Color", wxLIST_FORMAT_LEFT, 120);
    m_colorMappingsList->InsertColumn(3, "Preview", wxLIST_FORMAT_LEFT, 100);

    colorMappingsSizer->Add(m_colorMappingsList, 1, wxEXPAND | wxALL, 5);

    // Add/Edit color mapping controls
    wxStaticBoxSizer* editColorSizer =
        new wxStaticBoxSizer(wxVERTICAL, colorsPanel, "Add/Edit Color");
    wxFlexGridSizer* editGrid = new wxFlexGridSizer(3, 5, 10);
    editGrid->AddGrowableCol(1);

    // Value entry
    editGrid->Add(
        new wxStaticText(editColorSizer->GetStaticBox(), wxID_ANY, "Value:"), 0,
        wxALIGN_CENTER_VERTICAL);
    m_colorValueText = new wxTextCtrl(editColorSizer->GetStaticBox(), wxID_ANY);
    editGrid->Add(m_colorValueText, 1, wxEXPAND);
    editGrid->Add(
        new wxStaticText(editColorSizer->GetStaticBox(), wxID_ANY, ""),
        0); // Spacer

    // Background color picker
    editGrid->Add(new wxStaticText(
                      editColorSizer->GetStaticBox(), wxID_ANY, "Background:"),
        0, wxALIGN_CENTER_VERTICAL);
    wxBoxSizer* bgColorSizer = new wxBoxSizer(wxHORIZONTAL);
    m_bgColorSwatch = new wxPanel(editColorSizer->GetStaticBox(), wxID_ANY,
        wxDefaultPosition, wxSize(30, 20));
    m_bgColorSwatch->SetBackgroundColour(*wxWHITE);
    m_bgColorButton = new wxButton(editColorSizer->GetStaticBox(), wxID_ANY,
        "Choose...", wxDefaultPosition, wxDefaultSize);
    bgColorSizer->Add(m_bgColorSwatch, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    bgColorSizer->Add(m_bgColorButton, 0);
    editGrid->Add(bgColorSizer, 1, wxEXPAND);
    wxButton* defaultBgBtn = new wxButton(editColorSizer->GetStaticBox(),
        wxID_ANY, "Default", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    editGrid->Add(defaultBgBtn, 0);

    // Text color picker
    editGrid->Add(
        new wxStaticText(editColorSizer->GetStaticBox(), wxID_ANY, "Text:"), 0,
        wxALIGN_CENTER_VERTICAL);
    wxBoxSizer* fgColorSizer = new wxBoxSizer(wxHORIZONTAL);
    m_fgColorSwatch = new wxPanel(editColorSizer->GetStaticBox(), wxID_ANY,
        wxDefaultPosition, wxSize(30, 20));
    m_fgColorSwatch->SetBackgroundColour(*wxBLACK);
    m_fgColorButton = new wxButton(editColorSizer->GetStaticBox(), wxID_ANY,
        "Choose...", wxDefaultPosition, wxDefaultSize);
    fgColorSizer->Add(m_fgColorSwatch, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
    fgColorSizer->Add(m_fgColorButton, 0);
    editGrid->Add(fgColorSizer, 1, wxEXPAND);
    wxButton* defaultFgBtn = new wxButton(editColorSizer->GetStaticBox(),
        wxID_ANY, "Default", wxDefaultPosition, wxDefaultSize, wxBU_EXACTFIT);
    editGrid->Add(defaultFgBtn, 0);

    // Preview
    editGrid->Add(
        new wxStaticText(editColorSizer->GetStaticBox(), wxID_ANY, "Preview:"),
        0, wxALIGN_CENTER_VERTICAL);
    m_previewPanel = new wxPanel(editColorSizer->GetStaticBox(), wxID_ANY,
        wxDefaultPosition, wxSize(-1, 30));
    m_previewText = new wxStaticText(m_previewPanel, wxID_ANY, "Sample Text",
        wxDefaultPosition, wxDefaultSize,
        wxALIGN_CENTER_HORIZONTAL | wxST_NO_AUTORESIZE);
    wxBoxSizer* previewSizer = new wxBoxSizer(wxVERTICAL);
    previewSizer->AddStretchSpacer();
    previewSizer->Add(m_previewText, 0, wxALIGN_CENTER | wxALL, 5);
    previewSizer->AddStretchSpacer();
    m_previewPanel->SetSizer(previewSizer);
    editGrid->Add(m_previewPanel, 1, wxEXPAND);
    editGrid->Add(
        new wxStaticText(editColorSizer->GetStaticBox(), wxID_ANY, ""),
        0); // Spacer

    editColorSizer->Add(editGrid, 1, wxEXPAND | wxALL, 5);

    // Buttons for color operations
    wxBoxSizer* colorBtnSizer = new wxBoxSizer(wxHORIZONTAL);
    wxButton* addColorBtn =
        new wxButton(editColorSizer->GetStaticBox(), wxID_ANY, "Add");
    wxButton* updateColorBtn =
        new wxButton(editColorSizer->GetStaticBox(), wxID_ANY, "Update");
    wxButton* deleteColorBtn =
        new wxButton(editColorSizer->GetStaticBox(), wxID_ANY, "Delete");

    colorBtnSizer->Add(addColorBtn, 0, wxRIGHT, 5);
    colorBtnSizer->Add(updateColorBtn, 0, wxRIGHT, 5);
    colorBtnSizer->Add(deleteColorBtn, 0);

    editColorSizer->Add(colorBtnSizer, 0, wxALIGN_RIGHT | wxALL, 5);

    colorsSizer->Add(colorMappingsSizer, 1, wxEXPAND | wxALL, 10);
    colorsSizer->Add(editColorSizer, 0, wxEXPAND | wxALL, 10);

    colorsPanel->SetSizer(colorsSizer);

    // Add pages to notebook (only once!)
    notebook->AddPage(colorsPanel, "Colors");

    // Main layout
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    mainSizer->Add(notebook, 1, wxEXPAND | wxALL, 10);

    // Dialog buttons
    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->Add(new wxButton(this, wxID_OK, "Save"), 0, wxALL, 5);
    btnSizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxALL, 5);
    mainSizer->Add(btnSizer, 0, wxALIGN_RIGHT | wxBOTTOM | wxRIGHT, 10);

    SetSizerAndFit(mainSizer);

    // Bind events
    Bind(wxEVT_BUTTON, &ConfigEditorDialog::OnSave, this, wxID_OK);
    Bind(wxEVT_BUTTON, &ConfigEditorDialog::OnCancel, this, wxID_CANCEL);
    m_columnsList->Bind(
        wxEVT_LIST_ITEM_SELECTED, &ConfigEditorDialog::OnColumnSelected, this);
    addBtn->Bind(wxEVT_BUTTON, &ConfigEditorDialog::OnAddColumn, this);
    removeBtn->Bind(wxEVT_BUTTON, &ConfigEditorDialog::OnRemoveColumn, this);
    applyBtn->Bind(
        wxEVT_BUTTON, &ConfigEditorDialog::OnApplyColumnChanges, this);

    // Color tab events
    m_colorColumnChoice->Bind(
        wxEVT_CHOICE, &ConfigEditorDialog::OnColorColumnChanged, this);
    m_colorMappingsList->Bind(wxEVT_LIST_ITEM_SELECTED,
        &ConfigEditorDialog::OnColorMappingSelected, this);
    m_bgColorButton->Bind(
        wxEVT_BUTTON, &ConfigEditorDialog::OnBgColorButton, this);
    m_fgColorButton->Bind(
        wxEVT_BUTTON, &ConfigEditorDialog::OnFgColorButton, this);
    defaultBgBtn->Bind(
        wxEVT_BUTTON, &ConfigEditorDialog::OnDefaultBgColor, this);
    defaultFgBtn->Bind(
        wxEVT_BUTTON, &ConfigEditorDialog::OnDefaultFgColor, this);
    addColorBtn->Bind(
        wxEVT_BUTTON, &ConfigEditorDialog::OnAddColorMapping, this);
    updateColorBtn->Bind(
        wxEVT_BUTTON, &ConfigEditorDialog::OnUpdateColorMapping, this);
    deleteColorBtn->Bind(
        wxEVT_BUTTON, &ConfigEditorDialog::OnDeleteColorMapping, this);

    // Initialize UI
    RefreshColumns();

    if (m_colorColumnChoice->GetCount() > 0)
    {
        RefreshColorMappings();
    }

    // Initialize colors
    m_bgColor = *wxWHITE;
    m_fgColor = *wxBLACK;
    UpdateColorSwatches();
    UpdateColorPreview();
}

void ConfigEditorDialog::OnColumnSelected(wxListEvent& event)
{
    long index = event.GetIndex();
    if (index >= 0)
    {
        m_columnName->SetValue(m_columnsList->GetItemText(index, 1));
        m_columnVisible->SetValue(
            m_columnsList->GetItemText(index, 0) == "Yes");

        long width = 100;
        m_columnsList->GetItemText(index, 2).ToLong(&width);
        m_columnWidth->SetValue(width);

        m_selectedColumnIndex = index;
    }
}

void ConfigEditorDialog::OnAddColumn(wxCommandEvent& WXUNUSED(event))
{
    wxTextEntryDialog dialog(this, "Enter new column name:", "Add Column");
    if (dialog.ShowModal() == wxID_OK)
    {
        wxString name = dialog.GetValue();
        if (!name.empty())
        {
            auto& columns = config::GetConfig().GetMutableColumns();

            // Check for duplicate names
            bool isDuplicate = false;
            for (const auto& col : columns)
            {
                if (col.name == name.ToStdString())
                {
                    isDuplicate = true;
                    break;
                }
            }

            if (isDuplicate)
            {
                wxMessageBox("A column with this name already exists.",
                    "Duplicate Column", wxICON_ERROR);
                return;
            }

            // Create new column with default values
            config::ColumnConfig newColumn;
            newColumn.name = name.ToStdString();
            newColumn.isVisible = true;
            newColumn.width = 100;

            // Add to config
            columns.push_back(newColumn);

            // Update list control
            long itemIndex =
                m_columnsList->InsertItem(m_columnsList->GetItemCount(), "Yes");
            m_columnsList->SetItem(itemIndex, 1, name);
            m_columnsList->SetItem(itemIndex, 2, "100");
            m_columnsList->SetItemData(itemIndex, columns.size() - 1);

            // Select the new column
            m_columnsList->SetItemState(itemIndex,
                wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
        }
    }
}

void ConfigEditorDialog::OnRemoveColumn(wxCommandEvent& WXUNUSED(event))
{
    if (m_selectedColumnIndex < 0)
    {
        wxMessageBox("Please select a column to remove.", "No Selection",
            wxICON_INFORMATION);
        return;
    }

    auto& columns = config::GetConfig().GetMutableColumns();
    if (columns.size() <= 1)
    {
        wxMessageBox("Cannot remove the last column.", "Error", wxICON_ERROR);
        return;
    }

    wxString colName = m_columnsList->GetItemText(m_selectedColumnIndex, 1);
    if (wxMessageBox(
            wxString::Format(
                "Are you sure you want to remove column '%s'?", colName),
            "Confirm Removal", wxICON_QUESTION | wxYES_NO) != wxYES)
    {
        return;
    }

    // Find and remove from config
    size_t configIndex = m_columnsList->GetItemData(m_selectedColumnIndex);
    if (configIndex < columns.size())
    {
        columns.erase(columns.begin() + configIndex);
    }

    // Remove from list
    m_columnsList->DeleteItem(m_selectedColumnIndex);

    // Renumber remaining items
    for (int i = 0; i < m_columnsList->GetItemCount(); ++i)
    {
        m_columnsList->SetItemData(i, i);
    }

    // Reset selected index
    m_selectedColumnIndex = -1;
}

void ConfigEditorDialog::OnApplyColumnChanges(wxCommandEvent& WXUNUSED(event))
{
    if (m_selectedColumnIndex < 0)
    {
        wxMessageBox("Please select a column first.", "No Selection",
            wxICON_INFORMATION);
        return;
    }

    wxString newName = m_columnName->GetValue();
    if (newName.empty())
    {
        wxMessageBox("Column name cannot be empty.", "Error", wxICON_ERROR);
        return;
    }

    auto& columns = config::GetConfig().GetMutableColumns();
    size_t configIndex = m_columnsList->GetItemData(m_selectedColumnIndex);

    // Check for duplicate names if name changed
    if (newName != columns[configIndex].name)
    {
        for (size_t i = 0; i < columns.size(); ++i)
        {
            if (i != configIndex && columns[i].name == newName.ToStdString())
            {
                wxMessageBox("A column with this name already exists.",
                    "Duplicate Column", wxICON_ERROR);
                return;
            }
        }
    }

    // Update config
    columns[configIndex].name = newName.ToStdString();
    columns[configIndex].isVisible = m_columnVisible->GetValue();
    columns[configIndex].width = m_columnWidth->GetValue();

    // Update list item
    m_columnsList->SetItem(
        m_selectedColumnIndex, 0, m_columnVisible->GetValue() ? "Yes" : "No");
    m_columnsList->SetItem(m_selectedColumnIndex, 1, newName);
    m_columnsList->SetItem(
        m_selectedColumnIndex, 2, std::to_string(m_columnWidth->GetValue()));
}

void ConfigEditorDialog::OnSave(wxCommandEvent& WXUNUSED(event))
{
    auto& cfg = config::GetConfig();

    // Save general settings
    cfg.logLevel = m_logLevelChoice->GetStringSelection().ToStdString();
    cfg.xmlRootElement = m_inputs["xmlRootElement"]->GetValue().ToStdString();
    cfg.xmlEventElement = m_inputs["xmlEventElement"]->GetValue().ToStdString();

    cfg.SaveConfig();

    // Notify observers
    NotifyObservers();

    EndModal(wxID_OK);
}

void ConfigEditorDialog::OnCancel(wxCommandEvent&)
{
    EndModal(wxID_CANCEL);
}

void ConfigEditorDialog::OnOpenConfigFile(wxCommandEvent& WXUNUSED(event))
{
    const std::string& configPath = config::GetConfig().GetConfigFilePath();

    // Use wxLaunchDefaultApplication which works cross-platform
    if (!wxLaunchDefaultApplication(configPath))
    {
        wxMessageBox(
            "Could not open the configuration file with default editor.",
            "Error", wxICON_ERROR | wxOK);
    }
}

void ConfigEditorDialog::RefreshColumns()
{
    // Clear existing items
    m_columnsList->DeleteAllItems();

    // Get fresh column data from config
    const auto& columns = config::GetConfig().GetColumns();

    // Add columns in their actual array order - no sorting needed
    for (size_t i = 0; i < columns.size(); ++i)
    {
        const auto& column = columns[i];

        long itemIndex =
            m_columnsList->InsertItem(i, column.isVisible ? "Yes" : "No");
        m_columnsList->SetItem(itemIndex, 1, column.name);
        m_columnsList->SetItem(itemIndex, 2, std::to_string(column.width));
        m_columnsList->SetItemData(itemIndex, i);
    }

    // Reset selection
    m_selectedColumnIndex = -1;

    // Clear detail fields
    if (m_columnName)
        m_columnName->SetValue("");
    if (m_columnVisible)
        m_columnVisible->SetValue(false);
    if (m_columnWidth)
        m_columnWidth->SetValue(100);
}

void ConfigEditorDialog::OnColorColumnChanged(wxCommandEvent& WXUNUSED(event))
{
    RefreshColorMappings();
}

void ConfigEditorDialog::RefreshColorMappings()
{
    m_colorMappingsList->DeleteAllItems();

    wxString selectedColumn = m_colorColumnChoice->GetStringSelection();
    if (selectedColumn.IsEmpty())
        return;

    const auto& columnColors = config::GetConfig().columnColors;
    auto colIt = columnColors.find(selectedColumn.ToStdString());
    if (colIt != columnColors.end())
    {
        int idx = 0;
        for (const auto& mapping : colIt->second)
        {
            const std::string& value = mapping.first;
            const auto& colors = mapping.second;

            long itemIdx = m_colorMappingsList->InsertItem(idx, value);

            // Use existing string hex colors
            m_colorMappingsList->SetItem(itemIdx, 1, colors.bg);
            m_colorMappingsList->SetItem(itemIdx, 2, colors.fg);

            // Store original value for lookup
            m_colorMappingsList->SetItemData(itemIdx, idx);
            idx++;
        }
    }

    // Reset the color editor
    m_colorValueText->SetValue("");
    m_bgColor = *wxWHITE;
    m_fgColor = *wxBLACK;
    UpdateColorSwatches();
    UpdateColorPreview();
}

void ConfigEditorDialog::OnColorMappingSelected(wxListEvent& event)
{
    long idx = event.GetIndex();
    wxString value = m_colorMappingsList->GetItemText(idx, 0);
    wxString bgColorStr = m_colorMappingsList->GetItemText(idx, 1);
    wxString fgColorStr = m_colorMappingsList->GetItemText(idx, 2);

    m_colorValueText->SetValue(value);

    // Convert hex strings to wxColour
    m_bgColor = HexToColor(bgColorStr.ToStdString());
    m_fgColor = HexToColor(fgColorStr.ToStdString());

    UpdateColorSwatches();
    UpdateColorPreview();
}

void ConfigEditorDialog::UpdateColorSwatches()
{
    if (m_bgColorSwatch)
    {
        m_bgColorSwatch->SetBackgroundColour(m_bgColor);
        m_bgColorSwatch->Refresh();
    }

    if (m_fgColorSwatch)
    {
        m_fgColorSwatch->SetBackgroundColour(m_fgColor);
        m_fgColorSwatch->Refresh();
    }
}

void ConfigEditorDialog::OnBgColorButton(wxCommandEvent& WXUNUSED(event))
{
    wxColourData data;
    data.SetColour(m_bgColor);

    wxColourDialog dialog(this, &data);
    if (dialog.ShowModal() == wxID_OK)
    {
        m_bgColor = dialog.GetColourData().GetColour();
        UpdateColorSwatches();
        UpdateColorPreview();
    }
}

void ConfigEditorDialog::OnFgColorButton(wxCommandEvent& WXUNUSED(event))
{
    wxColourData data;
    data.SetColour(m_fgColor);

    wxColourDialog dialog(this, &data);
    if (dialog.ShowModal() == wxID_OK)
    {
        m_fgColor = dialog.GetColourData().GetColour();
        UpdateColorSwatches();
        UpdateColorPreview();
    }
}

void ConfigEditorDialog::UpdateColorPreview()
{
    m_previewPanel->SetBackgroundColour(m_bgColor);
    m_previewText->SetForegroundColour(m_fgColor);

    // Set preview text to the value if available
    if (!m_colorValueText->GetValue().IsEmpty())
    {
        m_previewText->SetLabel(m_colorValueText->GetValue());
    }
    else
    {
        m_previewText->SetLabel("Sample Text");
    }

    m_previewPanel->Refresh();
}

void ConfigEditorDialog::OnDefaultBgColor(wxCommandEvent& WXUNUSED(event))
{
    m_bgColor = *wxWHITE;
    UpdateColorSwatches();
    UpdateColorPreview();
}

void ConfigEditorDialog::OnDefaultFgColor(wxCommandEvent& WXUNUSED(event))
{
    m_fgColor = *wxBLACK;
    UpdateColorSwatches();
    UpdateColorPreview();
}

void ConfigEditorDialog::OnAddColorMapping(wxCommandEvent& WXUNUSED(event))
{
    wxString selectedColumn = m_colorColumnChoice->GetStringSelection();
    wxString value = m_colorValueText->GetValue();

    if (selectedColumn.IsEmpty())
    {
        wxMessageBox("Please select a column first.", "No Column Selected",
            wxICON_INFORMATION);
        return;
    }

    if (value.IsEmpty())
    {
        wxMessageBox("Please enter a value for the color mapping.",
            "Empty Value", wxICON_INFORMATION);
        return;
    }

    // Check if mapping already exists
    auto& columnColors = config::GetConfig().columnColors;
    auto& colMappings = columnColors[selectedColumn.ToStdString()];

    if (colMappings.find(value.ToStdString()) != colMappings.end())
    {
        wxMessageBox("A color mapping for this value already exists.\nUse "
                     "Update to modify it.",
            "Duplicate Mapping", wxICON_INFORMATION);
        return;
    }

    // Add the new mapping using the existing structs
    config::ColumnColor colorPair;
    colorPair.bg = ColorToHex(m_bgColor);
    colorPair.fg = ColorToHex(m_fgColor);

    colMappings[value.ToStdString()] = colorPair;

    // Refresh the list
    RefreshColorMappings();

    // Select the new item
    for (int i = 0; i < m_colorMappingsList->GetItemCount(); i++)
    {
        if (m_colorMappingsList->GetItemText(i, 0) == value)
        {
            m_colorMappingsList->SetItemState(i,
                wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
            break;
        }
    }
}

void ConfigEditorDialog::OnUpdateColorMapping(wxCommandEvent& WXUNUSED(event))
{
    wxString selectedColumn = m_colorColumnChoice->GetStringSelection();
    wxString value = m_colorValueText->GetValue();

    if (selectedColumn.IsEmpty() || value.IsEmpty())
    {
        wxMessageBox("Please select a column and enter a value.",
            "Missing Information", wxICON_INFORMATION);
        return;
    }

    // Update the mapping
    auto& columnColors = config::GetConfig().columnColors;
    auto& colMappings = columnColors[selectedColumn.ToStdString()];

    config::ColumnColor colorPair;
    colorPair.bg = ColorToHex(m_bgColor);
    colorPair.fg = ColorToHex(m_fgColor);

    colMappings[value.ToStdString()] = colorPair;

    // Refresh the list
    RefreshColorMappings();

    // Re-select the updated item
    for (int i = 0; i < m_colorMappingsList->GetItemCount(); i++)
    {
        if (m_colorMappingsList->GetItemText(i, 0) == value)
        {
            m_colorMappingsList->SetItemState(i,
                wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
            break;
        }
    }
}

void ConfigEditorDialog::OnDeleteColorMapping(wxCommandEvent& WXUNUSED(event))
{
    wxString selectedColumn = m_colorColumnChoice->GetStringSelection();

    if (selectedColumn.IsEmpty())
    {
        wxMessageBox("Please select a column first.", "No Column Selected",
            wxICON_INFORMATION);
        return;
    }

    long selected = -1;
    selected = m_colorMappingsList->GetNextItem(
        -1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);

    if (selected == -1)
    {
        wxMessageBox("Please select a color mapping to delete.", "No Selection",
            wxICON_INFORMATION);
        return;
    }

    wxString value = m_colorMappingsList->GetItemText(selected, 0);

    // Confirm deletion
    if (wxMessageBox(
            wxString::Format(
                "Are you sure you want to delete the color mapping for '%s'?",
                value),
            "Confirm Deletion", wxICON_QUESTION | wxYES_NO) != wxYES)
    {
        return;
    }

    // Delete the mapping
    auto& columnColors = config::GetConfig().columnColors;
    auto colIt = columnColors.find(selectedColumn.ToStdString());

    if (colIt != columnColors.end())
    {
        colIt->second.erase(value.ToStdString());

        // If no mappings left for this column, remove the column entry
        if (colIt->second.empty())
        {
            columnColors.erase(colIt);
        }
    }

    // Refresh the list
    RefreshColorMappings();
}

void ConfigEditorDialog::AddObserver(config::ConfigObserver* observer)
{
    if (observer)
    {
        // Prevent duplicates (fixes issue where observers were added multiple
        // times)
        if (std::find(m_observers.begin(), m_observers.end(), observer) ==
            m_observers.end())
        {
            m_observers.push_back(observer);
        }
    }
}

void ConfigEditorDialog::RemoveObserver(config::ConfigObserver* observer)
{
    if (observer)
    {
        m_observers.erase(
            std::remove(m_observers.begin(), m_observers.end(), observer),
            m_observers.end());
    }
}

void ConfigEditorDialog::NotifyObservers()
{
    for (auto* observer : m_observers)
    {
        if (observer)
        {
            observer->OnConfigChanged();
        }
    }
}

void ConfigEditorDialog::SwapListItems(
    unsigned int sourcePos, unsigned int targetPos)
{
    if (sourcePos == targetPos)
        return;

    auto& columns = config::GetConfig().GetMutableColumns();

    // Store source item's data
    wxString sourceVisible = m_columnsList->GetItemText(sourcePos, 0);
    wxString sourceName = m_columnsList->GetItemText(sourcePos, 1);
    wxString sourceWidth = m_columnsList->GetItemText(sourcePos, 2);
    long sourceConfigIndex = m_columnsList->GetItemData(sourcePos);

    // Store a copy of the source column from config
    config::ColumnConfig sourceColumn = columns[sourceConfigIndex];

    if (sourcePos < targetPos)
    {
        // Moving down: shift everything between source+1 and target up by one
        for (auto i = sourcePos; i < targetPos; i++)
        {
            // Shift UI items
            m_columnsList->SetItem(i, 0, m_columnsList->GetItemText(i + 1, 0));
            m_columnsList->SetItem(i, 1, m_columnsList->GetItemText(i + 1, 1));
            m_columnsList->SetItem(i, 2, m_columnsList->GetItemText(i + 1, 2));
            m_columnsList->SetItemData(i, m_columnsList->GetItemData(i + 1));
        }

        // Shift columns in the config array
        // Find the indices in the config array for both positions
        size_t configSourcePos = std::distance(columns.begin(),
            std::find_if(columns.begin(), columns.end(),
                [&](const config::ColumnConfig& col)
                { return col.name == sourceName.ToStdString(); }));

        // Perform rotation on the config array
        if (configSourcePos < columns.size() &&
            configSourcePos + 1 <= targetPos && targetPos < columns.size())
        {
            std::rotate(columns.begin() + configSourcePos,
                columns.begin() + configSourcePos + 1,
                columns.begin() + targetPos + 1);
        }
    }
    else
    {
        // Moving up: shift everything between target and source-1 down by one
        for (auto i = sourcePos; i > targetPos; i--)
        {
            // Shift UI items
            m_columnsList->SetItem(i, 0, m_columnsList->GetItemText(i - 1, 0));
            m_columnsList->SetItem(i, 1, m_columnsList->GetItemText(i - 1, 1));
            m_columnsList->SetItem(i, 2, m_columnsList->GetItemText(i - 1, 2));
            m_columnsList->SetItemData(i, m_columnsList->GetItemData(i - 1));
        }

        // Find the indices in the config array for both positions
        size_t configSourcePos = std::distance(columns.begin(),
            std::find_if(columns.begin(), columns.end(),
                [&](const config::ColumnConfig& col)
                { return col.name == sourceName.ToStdString(); }));

        // Perform rotation on the config array
        if (configSourcePos < columns.size() && targetPos <= configSourcePos &&
            targetPos < columns.size())
        {
            std::rotate(columns.begin() + targetPos,
                columns.begin() + configSourcePos,
                columns.begin() + configSourcePos + 1);
        }
    }

    // Place source item at target position
    m_columnsList->SetItem(targetPos, 0, sourceVisible);
    m_columnsList->SetItem(targetPos, 1, sourceName);
    m_columnsList->SetItem(targetPos, 2, sourceWidth);
    m_columnsList->SetItemData(targetPos, sourceConfigIndex);

    // Log the change
    util::Logger::Debug("Moved column {} from position {} to position {}",
        sourceName.ToStdString(), sourcePos, targetPos);

    // Make sure the OnSave method will save these changes
    config::GetConfig().SaveConfig();
}

void ConfigEditorDialog::OnMoveColumnUp(wxCommandEvent& WXUNUSED(event))
{
    if (m_selectedColumnIndex <= 0)
    {
        // Already at the top or nothing selected
        return;
    }

    // Get the items to swap
    int currentPos = m_selectedColumnIndex;
    int newPos = currentPos - 1;

    // Swap items in the list control and config
    SwapListItems(currentPos, newPos);

    // Update the selection to follow the moved item
    m_selectedColumnIndex = newPos;
    m_columnsList->SetItemState(newPos,
        wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
        wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
}

void ConfigEditorDialog::OnMoveColumnDown(wxCommandEvent& WXUNUSED(event))
{
    if (m_selectedColumnIndex < 0 ||
        m_selectedColumnIndex >= m_columnsList->GetItemCount() - 1)
    {
        // Nothing selected or already at the bottom
        return;
    }

    // Get the items to swap
    int currentPos = m_selectedColumnIndex;
    int newPos = currentPos + 1;

    // Swap items in the list control and config
    SwapListItems(currentPos, newPos);

    // Update the selection to follow the moved item
    m_selectedColumnIndex = newPos;
    m_columnsList->SetItemState(newPos,
        wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
        wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
}

// Add this helper method to the class implementation:
void ConfigEditorDialog::MoveColumnTo(int targetPos)
{
    if (m_selectedColumnIndex < 0 ||
        m_selectedColumnIndex >= m_columnsList->GetItemCount())
    {
        return; // Invalid selection
    }

    if (targetPos < 0 || targetPos >= m_columnsList->GetItemCount() ||
        targetPos == m_selectedColumnIndex)
    {
        return; // Invalid target position
    }

    // Move the column
    SwapListItems(m_selectedColumnIndex, targetPos);

    // Update the selection to follow the moved item
    m_selectedColumnIndex = targetPos;
    m_columnsList->SetItemState(targetPos,
        wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
        wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
}

wxColour ConfigEditorDialog::HexToColor(const std::string& hexColor)
{
    // Handle empty or invalid strings
    if (hexColor.empty() || hexColor[0] != '#' || hexColor.length() != 7)
        return wxColour(255, 255, 255); // Default white

    unsigned long r, g, b;
    r = strtoul(hexColor.substr(1, 2).c_str(), nullptr, 16);
    g = strtoul(hexColor.substr(3, 2).c_str(), nullptr, 16);
    b = strtoul(hexColor.substr(5, 2).c_str(), nullptr, 16);

    return wxColour(static_cast<unsigned char>(r), static_cast<unsigned char>(g), static_cast<unsigned char>(b));
}

std::string ConfigEditorDialog::ColorToHex(const wxColour& color)
{
    char hexStr[8];
    snprintf(hexStr, sizeof(hexStr), "#%02x%02x%02x", color.Red(),
        color.Green(), color.Blue());
    return std::string(hexStr);
}

// Add this method implementation

void ConfigEditorDialog::OnLogLevelChanged(wxCommandEvent& WXUNUSED(event))
{
    wxString selectedLevel = m_logLevelChoice->GetStringSelection();
    if (selectedLevel.IsEmpty())
        return;

    // Apply the log level immediately
    auto& cfg = config::GetConfig();
    std::string newLevel = selectedLevel.ToStdString();

    // Update config
    cfg.logLevel = newLevel;

    // Apply to spdlog
    spdlog::level::level_enum levelEnum = spdlog::level::info; // Default

    if (newLevel == "trace")
        levelEnum = spdlog::level::trace;
    else if (newLevel == "debug")
        levelEnum = spdlog::level::debug;
    else if (newLevel == "info")
        levelEnum = spdlog::level::info;
    else if (newLevel == "warning")
        levelEnum = spdlog::level::warn;
    else if (newLevel == "error")
        levelEnum = spdlog::level::err;
    else if (newLevel == "critical")
        levelEnum = spdlog::level::critical;
    else if (newLevel == "off")
        levelEnum = spdlog::level::off;

    // Set the global level
    spdlog::set_level(levelEnum);

    // Log that the level has been changed
    util::Logger::Info("Log level changed to: {}", newLevel);
}