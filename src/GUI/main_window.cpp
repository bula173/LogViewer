#include "main_window.hpp"
#include "events_virtual_list_control.hpp"

#include "MyApp.hpp"

namespace gui
{

  MainWindow::MainWindow(const wxString &title, const wxPoint &pos, const wxSize &size)
      : wxFrame(NULL, wxID_ANY, title, pos, size)
  {

    this->setupMenu();
    this->setupLayout();

    this->Bind(wxEVT_CLOSE_WINDOW, &MainWindow::OnClose, this);
  }

  void MainWindow::setupMenu()
  {

    wxMenu *menuFile = new wxMenu;
    menuFile->Append(ID_Hello, "&Populate dummy data.\tCtrl-H");
    menuFile->Append(wxID_OPEN);

    wxMenu *menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);

    wxMenu *menuView = new wxMenu;
    menuView->Append(wxID_VIEW_LIST, "Hide Search Result Panel", "Change view", wxITEM_CHECK);
    menuView->Append(ID_ViewLeftPanel, "Hide Left Panel", "Change view", wxITEM_CHECK);
    menuView->Append(ID_ViewRightPanel, "Hide Right Panel", "Change view", wxITEM_CHECK);

    wxMenuBar *menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
    menuBar->Append(menuHelp, "&Help");
    menuBar->Append(menuView, "&View");
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

    m_bottom_spliter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_BORDER | wxSP_LIVE_UPDATE);
    m_bottom_spliter->SetMinimumPaneSize(100);
    m_left_spliter = new wxSplitterWindow(m_bottom_spliter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_BORDER | wxSP_LIVE_UPDATE);
    m_left_spliter->SetMinimumPaneSize(200);
    m_rigth_spliter = new wxSplitterWindow(m_left_spliter, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxSP_BORDER | wxSP_LIVE_UPDATE);
    m_rigth_spliter->SetMinimumPaneSize(200);

    m_leftPanel = new wxPanel(m_left_spliter);
    m_searchResultPanel = new wxPanel(m_bottom_spliter);
    m_eventsListCtrl = new gui::EventsVirtualListControl(m_events, m_rigth_spliter); // main panel
    m_itemView = new gui::ItemVirtualListControl(m_events, m_rigth_spliter);

    m_bottom_spliter->SplitHorizontally(m_left_spliter, m_searchResultPanel, -1);
    m_bottom_spliter->SetSashGravity(0.2);

    m_left_spliter->SplitVertically(m_leftPanel, m_rigth_spliter, 1);
    m_left_spliter->SetSashGravity(0.2);

    m_rigth_spliter->SplitVertically(m_eventsListCtrl, m_itemView, -1);
    m_rigth_spliter->SetSashGravity(0.8);

    m_leftPanel->SetBackgroundColour(wxColor(200, 100, 200));
    m_searchResultPanel->SetBackgroundColour(wxColor(100, 200, 200));

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
    m_progressGauge->SetValue(0);

    m_processing = true;
    for (long i = 0; i < m_eventsNum; ++i)
    {
      if (i % 10)
      {
        m_events.AddEvent(db::Event(i, {{"timestamp", "dummyTimestamp"}, {"type", "dummyType"}, {"info", "dummyInfo"}, {"dummy", "dummy"}}));
      }
      else
      {
        m_events.AddEvent(db::Event(i, {{"timestamp", "dummyTimestamp"}, {"type", "dummyType"}, {"info", "dummyInfo"}}));
      }

      if (i % 100 == 0)
      {
        m_progressGauge->SetValue(i);
        wxYield();
      }

      if (m_closerequest)
      {
        m_processing = false;
        this->Destroy();
        return;
      }
    }

    SetStatusText("Data ready");
    m_processing = false;
  }
  void MainWindow::OnExit(wxCommandEvent &event)
  {
    Close(true);
  }

  void MainWindow::OnClose(wxCloseEvent &event)
  {

    if (m_processing)
    {
      event.Veto();
      m_closerequest = true;
    }
    else
    {
      this->Destroy();
    }
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

  void MainWindow::OnHideSearchResult(wxCommandEvent &event)
  {
    if (event.IsChecked())
    {
      m_searchResultPanel->Hide();
      m_bottom_spliter->Unsplit(m_searchResultPanel);
    }
    else
    {
      m_searchResultPanel->Show();
      m_bottom_spliter->SplitHorizontally(m_left_spliter, m_searchResultPanel, -1);
    }
    this->Layout();
  }

  void MainWindow::OnHideLeftPanel(wxCommandEvent &event)
  {
    if (event.IsChecked())
    {
      m_leftPanel->Hide();
      m_left_spliter->Unsplit(m_leftPanel);
    }
    else
    {
      m_leftPanel->Show();
      m_left_spliter->SplitVertically(m_leftPanel, m_rigth_spliter, 1);
    }
    this->Layout();
  }

  void MainWindow::OnHideRightPanel(wxCommandEvent &event)
  {
    if (event.IsChecked())
    {
      m_itemView->Hide();
      m_rigth_spliter->Unsplit(m_itemView);
    }
    else
    {
      m_itemView->Show();
      m_rigth_spliter->SplitVertically(m_eventsListCtrl, m_itemView, -1);
    }
    this->Layout();
  }

  wxBEGIN_EVENT_TABLE(MainWindow, wxFrame)
      EVT_MENU(ID_Hello, MainWindow::OnHello)
          EVT_MENU(wxID_VIEW_LIST, MainWindow::OnHideSearchResult)
              EVT_MENU(ID_ViewLeftPanel, MainWindow::OnHideLeftPanel)
                  EVT_MENU(ID_ViewRightPanel, MainWindow::OnHideRightPanel)
                      EVT_MENU(wxID_EXIT, MainWindow::OnExit)
                          EVT_MENU(wxID_ABOUT, MainWindow::OnAbout)
                              EVT_SIZE(MainWindow::OnSize)
                                  wxEND_EVENT_TABLE()

} // namespace gui