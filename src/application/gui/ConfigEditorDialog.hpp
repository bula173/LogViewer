#pragma once
#include "config/ConfigObserver.hpp"
#include <algorithm>
#include <map>
#include <string>
#include <vector>
#include <wx/checkbox.h>
#include <wx/clrpicker.h>
#include <wx/colordlg.h> // Add this for wxColourDialog
#include <wx/listctrl.h>
#include <wx/spinctrl.h>
#include <wx/wx.h>

class ConfigEditorDialog : public wxDialog
{
  public:
    ConfigEditorDialog(wxWindow* parent);

    // Observer pattern methods
    void AddObserver(config::ConfigObserver* observer);
    void RemoveObserver(config::ConfigObserver* observer);

  private:
    // Event handlers
    void OnSave(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);
    void OnOpenConfigFile(wxCommandEvent& event);
    void OnColumnSelected(wxListEvent& event);
    void OnAddColumn(wxCommandEvent& event);
    void OnRemoveColumn(wxCommandEvent& event);
    void OnApplyColumnChanges(wxCommandEvent& event);
    void OnMoveColumnUp(wxCommandEvent& event);
    void OnMoveColumnDown(wxCommandEvent& event);

    void NotifyObservers();
    void SwapListItems(unsigned int pos1, unsigned int pos2);
    void MoveColumnTo(int targetPos);

  private:
    std::map<std::string, wxTextCtrl*> m_inputs;
    std::vector<config::ConfigObserver*> m_observers;

    // Column management controls
    wxListCtrl* m_columnsList;
    wxTextCtrl* m_columnName;
    wxCheckBox* m_columnVisible;
    wxSpinCtrl* m_columnWidth;
    long m_selectedColumnIndex = -1;

    // Color configuration UI elements
    wxChoice* m_colorColumnChoice = nullptr;
    wxListCtrl* m_colorMappingsList = nullptr;
    wxTextCtrl* m_colorValueText = nullptr;

    // Color buttons and swatches
    wxButton* m_bgColorButton = nullptr;
    wxButton* m_fgColorButton = nullptr;
    wxPanel* m_bgColorSwatch = nullptr;
    wxPanel* m_fgColorSwatch = nullptr;

    // Color values
    wxColour m_bgColor;
    wxColour m_fgColor;

    // Preview elements
    wxPanel* m_previewPanel = nullptr;
    wxStaticText* m_previewText = nullptr;

    // Color-related methods
    void RefreshColorMappings();
    void UpdateColorSwatches();
    void UpdateColorPreview();
    void OnColorColumnChanged(wxCommandEvent& event);
    void OnColorMappingSelected(wxListEvent& event);
    void OnBgColorButton(wxCommandEvent& event);
    void OnFgColorButton(wxCommandEvent& event);
    void OnDefaultBgColor(wxCommandEvent& event);
    void OnDefaultFgColor(wxCommandEvent& event);
    void OnAddColorMapping(wxCommandEvent& event);
    void OnUpdateColorMapping(wxCommandEvent& event);
    void OnDeleteColorMapping(wxCommandEvent& event);
    void RefreshColumns();

    // Color utility functions
    wxColour HexToColor(const std::string& hexColor);
    std::string ColorToHex(const wxColour& color);

    // LogLevel choice
    wxChoice* m_logLevelChoice = nullptr;

    // Method declarations
    void OnLogLevelChanged(wxCommandEvent& event);
};