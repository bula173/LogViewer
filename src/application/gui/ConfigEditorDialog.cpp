#include "ConfigEditorDialog.hpp"
#include "config/Config.hpp"

ConfigEditorDialog::ConfigEditorDialog(wxWindow* parent)
    : wxDialog(parent, wxID_ANY, "Edit Configuration", wxDefaultPosition,
          wxSize(500, 400))
{
    auto& cfg = config::GetConfig();

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer* grid = new wxFlexGridSizer(2, 10, 10);

    // Example: Add text fields for some config parameters
    auto addField = [&](const std::string& label, const std::string& value)
    {
        grid->Add(new wxStaticText(this, wxID_ANY, label), 0,
            wxALIGN_CENTER_VERTICAL);
        wxTextCtrl* ctrl = new wxTextCtrl(this, wxID_ANY, value);
        grid->Add(ctrl, 1, wxEXPAND);
        m_inputs[label] = ctrl;
    };

    addField("logLevel", cfg.logLevel);
    addField("xmlRootElement", cfg.xmlRootElement);
    addField("xmlEventElement", cfg.xmlEventElement);
    // Add more fields as needed...

    grid->AddGrowableCol(1, 1);
    mainSizer->Add(grid, 1, wxALL | wxEXPAND, 10);

    wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
    btnSizer->Add(new wxButton(this, wxID_OK, "Save"), 0, wxALL, 5);
    btnSizer->Add(new wxButton(this, wxID_CANCEL, "Cancel"), 0, wxALL, 5);
    mainSizer->Add(btnSizer, 0, wxALIGN_RIGHT | wxBOTTOM | wxRIGHT, 10);

    SetSizerAndFit(mainSizer);

    Bind(wxEVT_BUTTON, &ConfigEditorDialog::OnSave, this, wxID_OK);
    Bind(wxEVT_BUTTON, &ConfigEditorDialog::OnCancel, this, wxID_CANCEL);
}

void ConfigEditorDialog::OnSave(wxCommandEvent&)
{
    auto& cfg = config::GetConfig();
    cfg.logLevel = m_inputs["logLevel"]->GetValue().ToStdString();
    cfg.xmlRootElement = m_inputs["xmlRootElement"]->GetValue().ToStdString();
    cfg.xmlEventElement = m_inputs["xmlEventElement"]->GetValue().ToStdString();
    // Save more fields as needed...

    cfg.SaveConfig();
    EndModal(wxID_OK);
}

void ConfigEditorDialog::OnCancel(wxCommandEvent&)
{
    EndModal(wxID_CANCEL);
}