#include "main_windows.hpp"

MainWindows::MainWindows(const wxString &title, const wxPoint &pos, const wxSize &size)
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
}
void MainWindows::OnExit(wxCommandEvent &event)
{
  Close(true);
}
void MainWindows::OnAbout(wxCommandEvent &event)
{
  wxMessageBox("This is a wxWidgets' Hello world sample",
               "About Hello World", wxOK | wxICON_INFORMATION);
}
void MainWindows::OnHello(wxCommandEvent &event)
{
  wxLogMessage("Hello world from wxWidgets!");
}

wxBEGIN_EVENT_TABLE(MainWindows, wxFrame)
    EVT_MENU(ID_Hello, MainWindows::OnHello)
        EVT_MENU(wxID_EXIT, MainWindows::OnExit)
            EVT_MENU(wxID_ABOUT, MainWindows::OnAbout)
                wxEND_EVENT_TABLE()