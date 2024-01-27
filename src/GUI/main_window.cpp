#include "main_window.hpp"
#include "events_virtual_list_control.hpp"

namespace gui
{

  MainWindow::MainWindow(const wxString &title, const wxPoint &pos, const wxSize &size)
      : wxFrame(NULL, wxID_ANY, title, pos, size)
  {

    this->setupMenu();
    this->setupLayout();

    m_events.RegisterOndDataUpdated(&(*m_eventsListCtrl));

    this->Layout();
  }

  void MainWindow::setupMenu()
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
  }

  void MainWindow::OnSize(wxSizeEvent &event)
  {

    event.Skip();

    if (m_progressGauge == nullptr)
      return;
    wxRect rect;
    GetStatusBar()->GetFieldRect(1, rect);
    m_progressGauge->SetSize(rect);
  }
  void MainWindow::setupLayout()
  {

    m_eventsListCtrl = new gui::EventsVirtualListControl(this, wxID_ANY, wxDefaultPosition, wxSize(800, 500), m_events);

    auto sizer = new wxBoxSizer(wxVERTICAL);
    sizer->Add(m_eventsListCtrl, 1, wxALL | wxEXPAND, 0);

    CreateToolBar();
    SetToolBar(GetToolBar());
    CreateStatusBar(2);

    wxRect rect;
    GetStatusBar()->GetFieldRect(1, rect);

    m_progressGauge = new wxGauge(GetStatusBar(), wxID_ANY, m_eventsNum, rect.GetPosition(), rect.GetSize(), wxGA_SMOOTH);
    m_progressGauge->SetValue(0);

    this->SetSizer(sizer);
  }

  void MainWindow::populateData()
  {

    SetStatusText("Loading ..");
    for (long i = 0; i < m_eventsNum; ++i)
    {
      m_events.AddEvent(db::Event(i));
      m_progressGauge->SetValue(i);

      if (i % 1000)
      {
        wxYield();
      }
    }
    SetStatusText("Data ready");
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
    populateData();
  }

  wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
      EVT_MENU(ID_Hello, MainWindow::OnHello)
          EVT_MENU(wxID_EXIT, MainWindow::OnExit)
              EVT_MENU(wxID_ABOUT, MainWindow::OnAbout)
                  EVT_SIZE(MainWindow::OnSize)
                      wxEND_EVENT_TABLE()

} // namespace gui