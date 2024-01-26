#include "main_window.hpp"
#include "events_virtual_list_control.hpp"

namespace gui
{

  MainWindow::MainWindow(const wxString &title, const wxPoint &pos, const wxSize &size)
      : wxFrame(NULL, wxID_ANY, title, pos, size)
  {
    wxMenu *menuFile = new wxMenu;
    menuFile->Append(ID_Hello, "&Hello...\tCtrl-H",
                     "Help string shown in status bar for this menu item");
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT);
    wxMenu *menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);
    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
    menuBar->Append(menuHelp, "&Help");
    SetMenuBar(menuBar);
    CreateStatusBar();
    SetStatusText("Welcome to wxWidgets!");

    m_eventsContainer = new gui::EventsVirtualListControl(this, wxID_ANY, wxDefaultPosition, wxSize(800, 500), m_events);
    m_eventsContainer->RefreshAfterUpdate();

    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_eventsContainer, 1, wxALL | wxEXPAND, 0);
    this->SetSizer(sizer);
    this->Layout();
  }
  void MainWindow::OnExit(wxCommandEvent &event)
  {
    Close(true);
  }
  void MainWindow::OnAbout(wxCommandEvent &event)
  {
    wxMessageBox("This is a wxWidgets' Hello world sample",
                 "About Hello World", wxOK | wxICON_INFORMATION);
  }
  void MainWindow::OnHello(wxCommandEvent &event)
  {
    wxLogMessage("Hello world from wxWidgets!");
  }

  wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
      EVT_MENU(ID_Hello, MainWindow::OnHello)
          EVT_MENU(wxID_EXIT, MainWindow::OnExit)
              EVT_MENU(wxID_ABOUT, MainWindow::OnAbout)
                  wxEND_EVENT_TABLE()

} // namespace gui