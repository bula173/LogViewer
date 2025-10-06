#include "FiltersPanel.hpp"
#include "FilterEditorDialog.hpp"
#include "MainWindow.hpp"
#include "filters/FilterManager.hpp"
#include <spdlog/spdlog.h>
#include <wx/filedlg.h> // For wxFileDialog
#include <wx/msgdlg.h>  // For wxMessageBox
#include <wx/statline.h>
#include <wx/stattext.h>

namespace gui
{

FiltersPanel::FiltersPanel(wxWindow* parent)
    : wxPanel(parent, wxID_ANY)
{
    CreateControls();
    BindEvents();
    RefreshFilters();
}

void FiltersPanel::CreateControls()
{
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // Header with title
    mainSizer->Add(new wxStaticText(this, wxID_ANY, "Filters:"), 0, wxALL, 5);

    // Create filters list
    m_filtersList = new wxListCtrl(this, wxID_ANY, wxDefaultPosition,
        wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
    m_filtersList->InsertColumn(0, "Enabled", wxLIST_FORMAT_CENTER, 60);
    m_filtersList->InsertColumn(1, "Name", wxLIST_FORMAT_LEFT, 120);
    m_filtersList->InsertColumn(2, "Column", wxLIST_FORMAT_LEFT, 80);
    m_filtersList->InsertColumn(3, "Pattern", wxLIST_FORMAT_LEFT, 150);

    mainSizer->Add(m_filtersList, 1, wxEXPAND | wxALL, 5);

    // Button row 1
    wxBoxSizer* buttonSizer1 = new wxBoxSizer(wxHORIZONTAL);

    m_addButton = new wxButton(this, wxID_ANY, "Add");
    m_editButton = new wxButton(this, wxID_ANY, "Edit");
    m_removeButton = new wxButton(this, wxID_ANY, "Remove");

    buttonSizer1->Add(m_addButton, 1, wxRIGHT, 5);
    buttonSizer1->Add(m_editButton, 1, wxRIGHT, 5);
    buttonSizer1->Add(m_removeButton, 1);

    mainSizer->Add(buttonSizer1, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Button row 2
    wxBoxSizer* buttonSizer2 = new wxBoxSizer(wxHORIZONTAL);

    m_applyButton = new wxButton(this, wxID_ANY, "Apply Filters");
    m_clearButton = new wxButton(this, wxID_ANY, "Clear All");

    buttonSizer2->Add(m_applyButton, 2, wxRIGHT, 5);
    buttonSizer2->Add(m_clearButton, 1);

    mainSizer->Add(buttonSizer2, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Add a separator between the existing buttons and the file operations
    mainSizer->Add(new wxStaticLine(this, wxID_ANY), 0, wxEXPAND | wxALL, 5);

    // File operations buttons
    wxBoxSizer* fileBtnSizer = new wxBoxSizer(wxHORIZONTAL);

    m_saveAsButton = new wxButton(this, wxID_ANY, "Save Filters As...");
    m_loadButton = new wxButton(this, wxID_ANY, "Load Filters...");

    fileBtnSizer->Add(m_saveAsButton, 1, wxRIGHT, 5);
    fileBtnSizer->Add(m_loadButton, 1);

    mainSizer->Add(fileBtnSizer, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);

    // Initial button states
    m_editButton->Disable();
    m_removeButton->Disable();

    SetSizer(mainSizer);
}

void FiltersPanel::BindEvents()
{
    // Button events
    m_addButton->Bind(wxEVT_BUTTON, &FiltersPanel::OnAddFilter, this);
    m_editButton->Bind(wxEVT_BUTTON, &FiltersPanel::OnEditFilter, this);
    m_removeButton->Bind(wxEVT_BUTTON, &FiltersPanel::OnRemoveFilter, this);
    m_applyButton->Bind(wxEVT_BUTTON, &FiltersPanel::OnApplyFilters, this);
    m_clearButton->Bind(wxEVT_BUTTON, &FiltersPanel::OnClearFilters, this);

    // List events
    m_filtersList->Bind(
        wxEVT_LIST_ITEM_SELECTED, &FiltersPanel::OnFilterSelected, this);
    m_filtersList->Bind(wxEVT_LIST_ITEM_ACTIVATED, &FiltersPanel::OnEditFilter,
        this); // Double-click to edit

    // Add this binding for mouse clicks
    m_filtersList->Bind(wxEVT_LEFT_DOWN, &FiltersPanel::OnListItemClick, this);

    // File operation events
    m_saveAsButton->Bind(wxEVT_BUTTON, &FiltersPanel::OnSaveFiltersAs, this);
    m_loadButton->Bind(wxEVT_BUTTON, &FiltersPanel::OnLoadFilters, this);
}

void FiltersPanel::RefreshFilters()
{
    m_filtersList->DeleteAllItems();

    // Get all filters from the manager
    const auto& filters = filters::FilterManager::getInstance().getFilters();

    int idx = 0;
    for (const auto& filter : filters)
    {
        if (!filter)
            continue;

        long itemIdx =
            m_filtersList->InsertItem(idx, filter->isEnabled ? "[x]" : "[ ]");
        m_filtersList->SetItem(itemIdx, 1, filter->name);
        m_filtersList->SetItem(itemIdx, 2, filter->columnName);
        m_filtersList->SetItem(itemIdx, 3, filter->pattern);
        m_filtersList->SetItemData(itemIdx, idx); // Store index

        // Visual indication of disabled filters
        if (!filter->isEnabled)
        {
            m_filtersList->SetItemTextColour(
                itemIdx, wxColour(150, 150, 150)); // Light gray
        }

        idx++;
    }

    // Reset selection
    m_selectedFilterIndex = -1;
    m_editButton->Disable();
    m_removeButton->Disable();
}

void FiltersPanel::ApplyFilters()
{
    // Find parent MainWindow to access the events container
    wxWindow* parent = GetParent();
    while (parent && !dynamic_cast<MainWindow*>(parent))
    {
        parent = parent->GetParent();
    }

    MainWindow* mainWindow = dynamic_cast<MainWindow*>(parent);
    if (!mainWindow)
    {
        spdlog::error("FiltersPanel: Cannot find MainWindow parent");
        return;
    }

    // Apply filters to the events list
    mainWindow->ApplyFilters();
}

void FiltersPanel::OnAddFilter(wxCommandEvent& WXUNUSED(event))
{
    FilterEditorDialog dialog(this);
    if (dialog.ShowModal() == wxID_OK)
    {
        auto filter = dialog.GetFilter();
        if (filter)
        {
            filters::FilterManager::getInstance().addFilter(filter);
            filters::FilterManager::getInstance().saveFilters();
            RefreshFilters();
        }
    }
}

void FiltersPanel::OnEditFilter(wxCommandEvent& WXUNUSED(event))
{
    if (m_selectedFilterIndex < 0)
        return;

    // Find the filter by name
    wxString filterName = m_filtersList->GetItemText(m_selectedFilterIndex, 1);
    auto filter = filters::FilterManager::getInstance().getFilterByName(
        filterName.ToStdString());

    if (!filter)
    {
        spdlog::error("FiltersPanel: Selected filter not found: {}",
            filterName.ToStdString());
        return;
    }

    FilterEditorDialog dialog(this, filter);
    if (dialog.ShowModal() == wxID_OK)
    {
        auto updatedFilter = dialog.GetFilter();
        if (updatedFilter)
        {
            filters::FilterManager::getInstance().updateFilter(updatedFilter);
            filters::FilterManager::getInstance().saveFilters();
            RefreshFilters();
            ApplyFilters();
        }
    }
}

void FiltersPanel::OnRemoveFilter(wxCommandEvent& WXUNUSED(event))
{
    if (m_selectedFilterIndex < 0)
        return;

    wxString filterName = m_filtersList->GetItemText(m_selectedFilterIndex, 1);

    if (wxMessageBox(
            wxString::Format(
                "Are you sure you want to remove the filter '%s'?", filterName),
            "Confirm Removal", wxYES_NO | wxICON_QUESTION) == wxYES)
    {
        filters::FilterManager::getInstance().removeFilter(
            filterName.ToStdString());
        filters::FilterManager::getInstance().saveFilters();
        RefreshFilters();
        ApplyFilters();
    }
}

void FiltersPanel::OnFilterSelected(wxListEvent& event)
{
    m_selectedFilterIndex = event.GetIndex();

    // Enable edit/remove buttons
    m_editButton->Enable();
    m_removeButton->Enable();
}

void FiltersPanel::OnFilterChecked(wxListEvent& event)
{
    int index = event.GetIndex();
    wxString filterName = m_filtersList->GetItemText(index, 1);

    // Toggle the checkmark
    bool isCurrentlyEnabled = !m_filtersList->GetItemText(index, 0).empty();
    bool newState = !isCurrentlyEnabled;

    // Update the UI
    m_filtersList->SetItem(index, 0, newState ? "[x]" : "[ ]");

    // Update the filter
    filters::FilterManager::getInstance().enableFilter(
        filterName.ToStdString(), newState);
    filters::FilterManager::getInstance().saveFilters();

    // Update item appearance
    if (newState)
    {
        m_filtersList->SetItemTextColour(index, *wxBLACK);
    }
    else
    {
        m_filtersList->SetItemTextColour(index, wxColour(150, 150, 150));
    }
}

void FiltersPanel::OnApplyFilters(wxCommandEvent& WXUNUSED(event))
{
    ApplyFilters();
}

void FiltersPanel::OnClearFilters(wxCommandEvent& WXUNUSED(event))
{
    if (wxMessageBox("Are you sure you want to disable all filters?",
            "Clear Filters", wxYES_NO | wxICON_QUESTION) == wxYES)
    {
        filters::FilterManager::getInstance().enableAllFilters(false);
        filters::FilterManager::getInstance().saveFilters();
        RefreshFilters();
        ApplyFilters();
    }
}

// Add this new method to handle clicking on items to toggle checkboxes:
void FiltersPanel::OnListItemClick(wxMouseEvent& event)
{
    // Get click position
    wxPoint pos = event.GetPosition();

    // Hit test to find the item and column under the mouse
    int flags = wxLIST_HITTEST_ONITEM;
    long item = m_filtersList->HitTest(pos, flags);

    if (item != wxNOT_FOUND)
    {
        // Get rectangle of first column (Enabled column)
        wxRect itemRect;
        m_filtersList->GetItemRect(item, itemRect);

        // Get column width to determine if click was in first column
        int colWidth = m_filtersList->GetColumnWidth(0);

        // If clicked on first column (Enabled column)
        if (pos.x <= colWidth)
        {
            // Get the filter name
            wxString filterName = m_filtersList->GetItemText(item, 1);

            // Toggle the checkmark
            bool isCurrentlyEnabled =
                !(m_filtersList->GetItemText(item, 0) == "[ ]");
            bool newState = !isCurrentlyEnabled;

            // Update the UI
            m_filtersList->SetItem(item, 0, newState ? "[x]" : "[ ]");

            // Update the filter
            filters::FilterManager::getInstance().enableFilter(
                filterName.ToStdString(), newState);
            filters::FilterManager::getInstance().saveFilters();

            // Update item appearance
            if (newState)
            {
                m_filtersList->SetItemTextColour(item, *wxBLACK);
            }
            else
            {
                m_filtersList->SetItemTextColour(item, wxColour(150, 150, 150));
            }
        }
    }

    // Continue event processing for selection, etc.
    event.Skip();
}

void FiltersPanel::OnSaveFiltersAs(wxCommandEvent& WXUNUSED(event))
{
    spdlog::info("OnSaveFiltersAs called");

    // Get top-level parent window for proper dialog centering
    wxWindow* topParent = wxGetTopLevelParent(this);

    // Create the file dialog with explicit parent
    wxFileDialog saveDialog(topParent, "Save Filters", "", "filters.json",
        "Filter files (*.json)|*.json", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);

    // For macOS, ensure dialog appears by explicitly yielding events
    wxYield();

    int result = saveDialog.ShowModal();

    if (result == wxID_CANCEL)
        return;

    wxString path = saveDialog.GetPath();
    spdlog::info("Selected path: {}", path.ToStdString());

    if (filters::FilterManager::getInstance().saveFiltersToPath(
            path.ToStdString()))
    {
        wxMessageBox(wxString::Format("Filters saved to:\n%s", path),
            "Filters Saved", wxOK | wxICON_INFORMATION, topParent);
        spdlog::info("Filters saved successfully");
    }
    else
    {
        wxMessageBox(wxString::Format("Failed to save filters to:\n%s", path),
            "Save Error", wxOK | wxICON_ERROR, topParent);
        spdlog::error("Failed to save filters");
    }
}

void FiltersPanel::OnLoadFilters(wxCommandEvent& WXUNUSED(event))
{
    wxFileDialog openDialog(this, "Load Filters", "", "",
        "Filter files (*.json)|*.json|All files (*.*)|*.*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (openDialog.ShowModal() == wxID_CANCEL)
        return;

    wxString path = openDialog.GetPath();

    if (wxMessageBox(
            "Loading filters will replace all current filters. Continue?",
            "Confirm Load", wxYES_NO | wxICON_QUESTION) != wxYES)
    {
        return;
    }

    if (filters::FilterManager::getInstance().loadFiltersFromPath(
            path.ToStdString()))
    {
        RefreshFilters();
        ApplyFilters();
        wxMessageBox(wxString::Format("Filters loaded from:\n%s", path),
            "Filters Loaded", wxOK | wxICON_INFORMATION);
    }
    else
    {
        wxMessageBox(wxString::Format("Failed to load filters from:\n%s", path),
            "Load Error", wxOK | wxICON_ERROR);
    }
}

} // namespace gui