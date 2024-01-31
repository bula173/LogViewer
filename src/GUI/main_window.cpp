#include "main_window.hpp"
#include "events_virtual_list_control.hpp"

#include <wx/splitter.h>

namespace gui
{

  MainWindow::MainWindow(const wxString &title, const wxPoint &pos, const wxSize &size)
      : wxFrame(NULL, wxID_ANY, title, pos, size)
  {

    this->setupMenu();
    this->setupLayout();
  }

  void MainWindow::setupMenu()
  {

    wxMenu *menuFile = new wxMenu;
    menuFile->Append(ID_Hello, "&Populate dummy data.\tCtrl-H",
                     "Populate dummy data");
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
    /*
    ________________________________________
    |              ToolBar                 |
    |      Tools       |     Search        |
    ________________________________________
    |            |            |            |
    | Left Panel | Main Panel | Right Panel|
    |            |            |            |
    ________________________________________
    |          Search result Panel         |
    _______________________________________
    |              Status Bar              |
    | General message | Progresbar         |
    ________________________________________
    */
    wxPanel *rigthPanel;
    wxPanel *leftPanel;
    wxPanel *searchResultPanel;

    wxSplitterWindow *bottom_spliter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_BORDER | wxSP_LIVE_UPDATE);
    bottom_spliter->SetMinimumPaneSize(100);
    wxSplitterWindow *left_spliter = new wxSplitterWindow(bottom_spliter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_BORDER | wxSP_LIVE_UPDATE);
    left_spliter->SetMinimumPaneSize(200);
    wxSplitterWindow *rigth_spliter = new wxSplitterWindow(left_spliter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_BORDER | wxSP_LIVE_UPDATE);
    rigth_spliter->SetMinimumPaneSize(200);

    leftPanel = new wxPanel(left_spliter);
    rigthPanel = new wxPanel(rigth_spliter);
    searchResultPanel = new wxPanel(bottom_spliter);
    m_eventsListCtrl = new gui::EventsVirtualListControl(rigth_spliter, wxID_ANY, wxDefaultPosition, wxDefaultSize, m_events); //main panel

    bottom_spliter->SplitHorizontally(left_spliter, searchResultPanel,-1);
    bottom_spliter->SetSashGravity(0.2);

    left_spliter->SplitVertically(leftPanel, rigth_spliter, 1);
    left_spliter->SetSashGravity(0.2);

    rigth_spliter->SplitVertically(m_eventsListCtrl, rigthPanel, -1);
    rigth_spliter->SetSashGravity(0.8);

    leftPanel->SetBackgroundColour(wxColor(200, 100, 200));
    rigthPanel->SetBackgroundColour(wxColor(100, 100, 200));
    searchResultPanel->SetBackgroundColour(wxColor(100, 200, 200));

    CreateToolBar();
    setupStatusBar();
  }

  void MainWindow::setupStatusBar()
  {

    CreateStatusBar(2); // 1 - Message, 2-Progressbar

    wxRect rect;
    GetStatusBar()->GetFieldRect(1, rect);

    m_progressGauge = new wxGauge(GetStatusBar(), wxID_ANY, m_eventsNum, rect.GetPosition(), rect.GetSize(), wxGA_SMOOTH);
    m_progressGauge->SetValue(0);
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