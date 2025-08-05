#pragma once
#include <map>
#include <string>
#include <wx/wx.h>

class ConfigEditorDialog : public wxDialog
{
  public:
    ConfigEditorDialog(wxWindow* parent);

  private:
    void OnSave(wxCommandEvent& event);
    void OnCancel(wxCommandEvent& event);

    std::map<std::string, wxTextCtrl*> m_inputs;
};