#include "gui/MainWindow.hpp"
#include "gui/EventsVirtualListControl.hpp"


#include <wx/app.h>

namespace gui
{

MainWindow::MainWindow(const wxString& title, const wxPoint& pos,
    const wxSize& size, const Version::Version& version)
    : wxFrame(NULL, wxID_ANY, title, pos, size)
    , m_version(version)
{

    this->setupMenu();
    this->setupLayout();

    this->Bind(wxEVT_CLOSE_WINDOW, &MainWindow::OnClose, this);
    this->Bind(wxEVT_MENU, &MainWindow::OnHello, this, ID_Hello);
    this->Bind(
        wxEVT_MENU, &MainWindow::OnHideSearchResult, this, wxID_VIEW_LIST);
    this->Bind(
        wxEVT_MENU, &MainWindow::OnHideLeftPanel, this, ID_ViewLeftPanel);
    this->Bind(
        wxEVT_MENU, &MainWindow::OnHideRightPanel, this, ID_ViewRightPanel);
    this->Bind(wxEVT_MENU, &MainWindow::OnExit, this, wxID_EXIT);
    this->Bind(wxEVT_MENU, &MainWindow::OnAbout, this, wxID_ABOUT);
    this->Bind(wxEVT_SIZE, &MainWindow::OnSize, this);
    this->Bind(wxEVT_MENU, &MainWindow::OnOpen, this, wxID_OPEN);
}

void MainWindow::setupMenu()
{

    wxMenu* menuFile = new wxMenu;
    menuFile->Append(ID_Hello, "&Populate dummy data.\tCtrl-H");
    menuFile->Append(wxID_OPEN, "&Open...\tCtrl-O", "Open a file");
#ifndef __WXMAC__ // Add "About" manually for non-macOS
    wxMenu* menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);
#endif

    wxMenu* menuView = new wxMenu;
    menuView->Append(wxID_VIEW_LIST, "Hide Search Result Panel", "Change view",
        wxITEM_CHECK);
    menuView->Append(
        ID_ViewLeftPanel, "Hide Left Panel", "Change view", wxITEM_CHECK);
    menuView->Append(
        ID_ViewRightPanel, "Hide Right Panel", "Change view", wxITEM_CHECK);

    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
#ifndef __WXMAC__ // Add "About" manually for non-macOS
    menuBar->Append(menuHelp, "&Help");
#endif
    menuBar->Append(menuView, "&View");
    SetMenuBar(menuBar);
}

void MainWindow::OnSize(wxSizeEvent& event)
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

    m_bottom_spliter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition,
        wxDefaultSize, wxSP_BORDER | wxSP_LIVE_UPDATE);
    m_bottom_spliter->SetMinimumPaneSize(100);
    m_left_spliter = new wxSplitterWindow(m_bottom_spliter, wxID_ANY,
        wxDefaultPosition, wxDefaultSize, wxSP_BORDER | wxSP_LIVE_UPDATE);
    m_left_spliter->SetMinimumPaneSize(200);
    m_rigth_spliter = new wxSplitterWindow(m_left_spliter, wxID_ANY,
        wxDefaultPosition, wxDefaultSize, wxSP_BORDER | wxSP_LIVE_UPDATE);
    m_rigth_spliter->SetMinimumPaneSize(200);

    m_leftPanel = new wxPanel(m_left_spliter);
    m_searchResultPanel = new wxPanel(m_bottom_spliter);
    m_eventsListCtrl = new gui::EventsVirtualListControl(
        m_events, m_rigth_spliter); // main panel
    m_itemView = new gui::ItemVirtualListControl(m_events, m_rigth_spliter);

    m_bottom_spliter->SplitHorizontally(
        m_left_spliter, m_searchResultPanel, -1);
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

    m_progressGauge = new wxGauge(GetStatusBar(), wxID_ANY, m_eventsNum,
        rect.GetPosition(), rect.GetSize(), wxGA_SMOOTH);
    m_progressGauge->SetValue(0);
}

void MainWindow::populateData()
{

    m_events.Clear();

    SetStatusText("Loading ..");
    m_progressGauge->SetValue(0);

    m_processing = true;
    for (long i = 0; i < m_eventsNum; ++i)
    {
        if (i % 10)
        {
            m_events.AddEvent(db::LogEvent(i,
                {{"timestamp", "dummyTimestamp"}, {"type", "dummyType"},
                    {"info", "dummyInfo"}, {"dummy", "dummy"}}));
        }
        else
        {
            m_events.AddEvent(db::LogEvent(i,
                {{"timestamp", "dummyTimestamp"}, {"type", "dummyType"},
                    {"info", "dummyInfo"}}));
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
void MainWindow::OnExit(wxCommandEvent& event)
{
    Close(true);
}

void MainWindow::OnClose(wxCloseEvent& event)
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

void MainWindow::OnAbout(wxCommandEvent& event)
{
    wxMessageBox(
        "This is simple log viewer which parse logs and display in table view.",
        "About" + m_version.asLongStr(), wxOK | wxICON_INFORMATION);
}
void MainWindow::OnHello(wxCommandEvent& event)
{
    populateData();
}

void MainWindow::OnHideSearchResult(wxCommandEvent& event)
{
    if (event.IsChecked())
    {
        m_searchResultPanel->Hide();
        m_bottom_spliter->Unsplit(m_searchResultPanel);
    }
    else
    {
        m_searchResultPanel->Show();
        m_bottom_spliter->SplitHorizontally(
            m_left_spliter, m_searchResultPanel, -1);
    }
    this->Layout();
}

void MainWindow::OnHideLeftPanel(wxCommandEvent& event)
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

void MainWindow::OnHideRightPanel(wxCommandEvent& event)
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

void MainWindow::OnOpen(wxCommandEvent& event)
{
    wxFileDialog openFileDialog(this, _("Open XML file"), "", "",
        "XML files (*.xml)|*.xml|All files (*.*)|*.*",
        wxFD_OPEN | wxFD_FILE_MUST_EXIST);

    if (openFileDialog.ShowModal() == wxID_OK)
    {
        // Proceed loading the file chosen by the user
        std::string filePath = openFileDialog.GetPath().ToStdString();
        CallAfter([this, filePath]() { ParseData(filePath); });
    }
}

void MainWindow::ParseData(const std::string filePath)
{
    // Add your file processing code here
    // For example, you can call your XML parser to parse the selected file
    m_events.Clear();
    m_parser = std::make_shared<parser::XmlParser>();

    m_parser->RegisterObserver(this);

    SetStatusText("Loading ..");
    m_progressGauge->SetValue(0);

    m_processing = true;

    m_progressGauge->SetRange(m_parser->GetTotalProgress());
    m_parser->ParseData(filePath);
    SetStatusText("Data ready");
}

// Data Observer methods
void MainWindow::ProgressUpdated()
{
    int progress = m_parser->GetCurrentProgress();
    m_progressGauge->SetValue(progress);
    wxYield();

    if (progress >= m_progressGauge->GetRange())
    {
        m_processing = false;
    }

    if (m_closerequest)
    {
        this->Destroy();
        return;
    }
}

void MainWindow::NewEventFound(db::LogEvent&& event)
{
    m_events.AddEvent(std::move(event));
}

} // namespace gui