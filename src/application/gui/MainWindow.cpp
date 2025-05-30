#include "gui/MainWindow.hpp"
#include "config/Config.hpp"
#include "gui/EventsVirtualListControl.hpp"

#include <spdlog/spdlog.h>
#include <wx/app.h>
#include <wx/settings.h>
// std
#include <filesystem>
#include <set>

namespace gui
{

MainWindow::MainWindow(const wxString& title, const wxPoint& pos,
    const wxSize& size, const Version::Version& version)
    : wxFrame(NULL, wxID_ANY, title, pos, size)
    , m_version(version)
{
    spdlog::info(
        "MainWindow created with title: {}", std::string(title.mb_str()));
    this->setupMenu();
    this->setupLayout();

    this->Bind(wxEVT_CLOSE_WINDOW, &MainWindow::OnClose, this);
#ifndef NDEBUG
    this->Bind(wxEVT_MENU, &MainWindow::OnPopulateDummyData, this,
        ID_PopulateDummyData);
#endif
    this->Bind(
        wxEVT_MENU, &MainWindow::OnHideSearchResult, this, wxID_VIEW_LIST);
    this->Bind(
        wxEVT_MENU, &MainWindow::OnHideLeftPanel, this, ID_ViewLeftPanel);
    this->Bind(
        wxEVT_MENU, &MainWindow::OnHideRightPanel, this, ID_ViewRightPanel);
    this->Bind(wxEVT_MENU, &MainWindow::OnExit, this, wxID_EXIT);
    this->Bind(wxEVT_MENU, &MainWindow::OnAbout, this, wxID_ABOUT);
    this->Bind(wxEVT_SIZE, &MainWindow::OnSize, this);
    this->Bind(wxEVT_MENU, &MainWindow::OnOpenFile, this, wxID_OPEN);
    this->Bind(wxEVT_MENU, &MainWindow::OnClearParser, this, ID_ParserClear);
    this->Bind(
        wxEVT_MENU, &MainWindow::OnOpenConfigEditor, this, ID_ConfigEditor);
    this->Bind(wxEVT_MENU, &MainWindow::OnOpenAppLog, this, ID_AppLogViewer);
    this->Bind(
        wxEVT_BUTTON, &MainWindow::OnSearch, this, m_searchButton->GetId());
    m_searchBox->Bind(wxEVT_TEXT_ENTER, &MainWindow::OnSearch, this);
    m_searchResultsList->Bind(wxEVT_DATAVIEW_ITEM_ACTIVATED,
        &MainWindow::OnSearchResultActivated, this);

    m_typeFilter->Bind(wxEVT_CHOICE, &MainWindow::OnFilterChanged, this);
    m_timestampFilter->Bind(wxEVT_CHOICE, &MainWindow::OnFilterChanged, this);
    this->Bind(wxEVT_MENU, &MainWindow::OnThemeLight, this, 2001);
    this->Bind(wxEVT_MENU, &MainWindow::OnThemeDark, this, 2002);
}

void MainWindow::setupMenu()
{

    // Create the menu bar
    // File menu
    wxMenu* menuFile = new wxMenu;
#ifndef NDEBUG
    menuFile->Append(ID_PopulateDummyData, "&Populate dummy data.\tCtrl-D",
        "Populate dummy data");
#endif
    menuFile->Append(wxID_OPEN, "&Open...\tCtrl-O", "Open a file");
#ifndef __WXMAC__ // Add "About" manually for non-macOS
    wxMenu* menuHelp = new wxMenu;
    menuHelp->Append(wxID_ABOUT);
#endif
    // View menu
    wxMenu* menuView = new wxMenu;
    menuView->Append(wxID_VIEW_LIST, "Hide Search Result Panel", "Change view",
        wxITEM_CHECK);
    menuView->Append(
        ID_ViewLeftPanel, "Hide Left Panel", "Change view", wxITEM_CHECK);
    menuView->Append(
        ID_ViewRightPanel, "Hide Right Panel", "Change view", wxITEM_CHECK);

    wxMenu* menuTheme = new wxMenu;
    menuTheme->AppendRadioItem(2001, "Light Theme");
    menuTheme->AppendRadioItem(2002, "Dark Theme");

    // Parser menu
    wxMenu* menuParser = new wxMenu;
    menuParser->Append(ID_ParserClear, "Clear", "Parser");

    // Config menu
    menuFile->Append(
        ID_ConfigEditor, "Edit &Config...\tCtrl+E", "Open Config Editor");

    menuFile->Append(
        ID_AppLogViewer, "Open &Application Logs...\tCtrl+L", "Open App Logs");

    // Main menu bar
    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
#ifndef __WXMAC__ // Add "About" manually for non-macOS
    menuBar->Append(menuHelp, "&Help");
#endif
    menuBar->Append(menuParser, "&Parser");
    menuBar->Append(menuTheme, "&Theme");
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

void MainWindow::ApplyDarkThemeRecursive(wxWindow* window)
{
    if (!window)
        return;

    window->SetBackgroundColour(wxColour(50, 50, 50));
    window->SetForegroundColour(wxColour(220, 220, 220));

    for (auto child : window->GetChildren())
    {
        ApplyDarkThemeRecursive(child);
    }
}

void MainWindow::ApplyLightThemeRecursive(wxWindow* window)
{
    if (!window)
        return;

    window->SetBackgroundColour(wxColour(220, 220, 220));
    window->SetForegroundColour(wxColour(30, 30, 30));

    for (auto child : window->GetChildren())
    {
        ApplyLightThemeRecursive(child);
    }
}

void MainWindow::OnThemeLight(wxCommandEvent&)
{
    spdlog::info("Switching to Light Theme");
    ApplyLightThemeRecursive(this);
    this->Refresh();
    this->Update();
}

void MainWindow::OnThemeDark(wxCommandEvent&)
{
    spdlog::info("Switching to Dark Theme");
    ApplyDarkThemeRecursive(this);
    this->Refresh();
    this->Update();
}

void MainWindow::setupLayout()
{
    spdlog::info("Setting up layout...");
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

    // Create a vertical sizer for the whole search panel
    auto* searchSizer = new wxBoxSizer(wxVERTICAL);

    // Create a horizontal sizer for the search box and button
    auto* searchBarSizer = new wxBoxSizer(wxHORIZONTAL);
    m_searchBox = new wxTextCtrl(m_searchResultPanel, wxID_ANY, "",
        wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    m_searchButton = new wxButton(m_searchResultPanel, wxID_ANY, "Search");
    searchBarSizer->Add(m_searchBox, 1, wxEXPAND | wxALL, 5);
    searchBarSizer->Add(m_searchButton, 0, wxEXPAND | wxALL, 5);

    m_searchResultsList = new wxDataViewListCtrl(m_searchResultPanel, wxID_ANY,
        wxDefaultPosition, wxDefaultSize, wxDV_ROW_LINES | wxDV_VERT_RULES);

    m_searchResultsList->AppendTextColumn("Event ID");
    m_searchResultsList->AppendTextColumn("Match");


    searchSizer->Add(searchBarSizer, 0, wxEXPAND);
    searchSizer->Add(m_searchResultsList, 1, wxEXPAND | wxALL, 5);

    m_searchResultPanel->SetSizer(searchSizer);

    m_eventsListCtrl =
        new gui::EventsVirtualListControl(m_events, m_rigth_spliter);
    m_itemView = new gui::ItemVirtualListControl(m_events, m_rigth_spliter);

    m_bottom_spliter->SplitHorizontally(
        m_left_spliter, m_searchResultPanel, -1);
    m_bottom_spliter->SetSashGravity(0.3);

    m_left_spliter->SplitVertically(m_leftPanel, m_rigth_spliter, 1);
    m_left_spliter->SetSashGravity(0.2);

    m_rigth_spliter->SplitVertically(m_eventsListCtrl, m_itemView, -1);
    m_rigth_spliter->SetSashGravity(0.8);

    auto* filterSizer = new wxBoxSizer(wxVERTICAL);

    // Example: Type filter
    m_typeFilter = new wxChoice(m_leftPanel, wxID_ANY);
    filterSizer->Add(
        new wxStaticText(m_leftPanel, wxID_ANY, "Type:"), 0, wxALL, 5);
    filterSizer->Add(m_typeFilter, 0, wxEXPAND | wxALL, 5);


    // Example: Timestamp filter
    m_timestampFilter = new wxChoice(m_leftPanel, wxID_ANY);
    filterSizer->Add(
        new wxStaticText(m_leftPanel, wxID_ANY, "Timestamp:"), 0, wxALL, 5);
    filterSizer->Add(m_timestampFilter, 0, wxEXPAND | wxALL, 5);

    m_leftPanel->SetSizer(filterSizer);

    CreateToolBar();
    setupStatusBar();
    spdlog::info("Layout setup complete.");
}

void MainWindow::OnSearch(wxCommandEvent& WXUNUSED(event))
{
    wxString query = m_searchBox->GetValue();
    m_searchResultsList->DeleteAllItems();

    for (unsigned long i = 0; i < m_events.Size(); ++i)
    {
        const auto& event = m_events.GetEvent(i);
        for (const auto& item : event.getEventItems())
        {
            if (item.second.find(query.ToStdString()) != std::string::npos)
            {
                wxVector<wxVariant> row;
                row.push_back(wxVariant(std::to_string(event.getId())));
                row.push_back(wxVariant(item.second));
                m_searchResultsList->AppendItem(row);
                break;
            }
        }
    }
}

void MainWindow::OnSearchResultActivated(wxDataViewEvent& event)
{
    int itemIndex = m_searchResultsList->ItemToRow(event.GetItem());
    wxVariant value;
    m_searchResultsList->GetValue(value, itemIndex, 0); // column 0 = Event ID
    wxString eventIdStr = value.GetString();
    long eventId;
    if (!eventIdStr.ToLong(&eventId))
    {
        spdlog::warn(
            "Could not convert event ID from search result to long: {}",
            eventIdStr.ToStdString());
        return;
    }

    // Find the index of the event with this ID in m_events
    for (unsigned long i = 0; i < m_events.Size(); ++i)
    {
        if (m_events.GetEvent(i).getId() == eventId)
        {
            m_events.SetCurrentItem(i); // This will update m_eventsListCtrl
            break;
        }
    }
}

void MainWindow::setupStatusBar()
{
    spdlog::info("Setting up status bar...");

    CreateStatusBar(2); // 1 - Message, 2-Progressbar

    wxRect rect;
    GetStatusBar()->GetFieldRect(1, rect);

    m_progressGauge = new wxGauge(GetStatusBar(), wxID_ANY, 1,
        rect.GetPosition(), rect.GetSize(), wxGA_SMOOTH);
    m_progressGauge->SetValue(0);

    spdlog::info("Status bar setup complete.");
}
#ifndef NDEBUG
void MainWindow::OnPopulateDummyData(wxCommandEvent& WXUNUSED(event))
{
    spdlog::info("Populating dummy data...");
    m_searchResultsList->DeleteAllItems();
    m_searchBox->SetValue("");
    m_events.Clear();
    SetStatusText("Loading ..");
    m_progressGauge->SetValue(0);

    m_processing = true;
    for (long i = 0; i < 1000; ++i)
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

    spdlog::info("Dummy data population complete.");
}
#endif

void MainWindow::OnExit(wxCommandEvent& WXUNUSED(event))
{
    spdlog::info("Exit requested.");
    Close(true);
}

void MainWindow::OnClose(wxCloseEvent& event)
{
    spdlog::info("Window close event triggered.");
    if (m_processing)
    {
        spdlog::warn("Close requested while processing. Vetoing close.");
        event.Veto();
        m_closerequest = true;
    }
    else
    {
        spdlog::info("Destroying window.");
        this->Destroy();
    }
}

void MainWindow::OnAbout(wxCommandEvent& WXUNUSED(event))
{
    spdlog::info("About dialog opened.");
    wxMessageBox("This is simple log viewer which parse logs and display "
                 "in table view.",
        "About" + m_version.asLongStr(), wxOK | wxICON_INFORMATION);
}

void MainWindow::OnClearParser(wxCommandEvent& WXUNUSED(event))
{
    spdlog::info("Parser clear triggered.");
    m_searchResultsList->DeleteAllItems();
    m_searchBox->SetValue("");
    m_events.Clear();
    m_progressGauge->SetValue(0);
    SetStatusText("Data cleared");
}

void MainWindow::OnHideSearchResult(wxCommandEvent& event)
{
    spdlog::info("Toggling Search Result Panel: {}",
        event.IsChecked() ? "Hide" : "Show");
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
    spdlog::info(
        "Toggling Left Panel: {}", event.IsChecked() ? "Hide" : "Show");
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
    spdlog::info(
        "Toggling Right Panel: {}", event.IsChecked() ? "Hide" : "Show");
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

void MainWindow::OnOpenFile(wxCommandEvent& WXUNUSED(event))
{
    spdlog::info("Open file dialog triggered.");
    wxFileDialog openFileDialog(this, _("Open log file"), "", "",
        "XML files (*.xml)|*.xml|All files (*.*)|*.*");

    auto result = openFileDialog.ShowModal();

    if (result == wxID_OK)
    {
        std::string filePath = openFileDialog.GetPath().ToStdString();
        spdlog::info("File selected: {}", filePath);
        CallAfter([this, filePath]() { ParseData(filePath); });
        CallAfter([this, filePath]() { UpdateFilters(); });
    }
    else
    {
        spdlog::warn("Open file dialog canceled. Reason: {}", result);
        return;
    }
}

void MainWindow::OnOpenConfigEditor(wxCommandEvent& WXUNUSED(event))
{
    spdlog::info("Config Editor menu selected");

    auto& config = config::GetConfig();
    auto& configPath = config.GetConfigFilePath();
    if (!std::filesystem::exists(configPath))
    {
        spdlog::error("Config file does not exist: {}", configPath);
        wxMessageBox(
            "Config file does not exist.", "Error", wxOK | wxICON_ERROR);
        return;
    }
    spdlog::info("Opening config file: {}", configPath);
    // Use wxLaunchDefaultApplication to open the config file in the default
    // editor
    wxLaunchDefaultApplication(configPath);

    // Option 2: Show a custom dialog (implement your own
    // ConfigEditorDialog) ConfigEditorDialog dlg(this, configPath);
    // dlg.ShowModal();
}

void MainWindow::OnFilterChanged(wxCommandEvent&)
{
    wxString selectedType = m_typeFilter->GetStringSelection();
    wxString selectedTimestamp = m_timestampFilter->GetStringSelection();

    // Filter events and update m_eventsListCtrl
    std::vector<int> filteredIndices;
    for (unsigned long i = 0; i < m_events.Size(); ++i)
    {
        const auto& event = m_events.GetEvent(i);
        bool match = true;
        if (selectedType != "All" && event.findByKey("type") != selectedType)
            match = false;
        if (selectedTimestamp != "All" &&
            event.findByKey("timestamp") != selectedTimestamp)
            match = false;
        if (match)
            filteredIndices.push_back(i);
    }
}

void MainWindow::OnOpenAppLog(wxCommandEvent& WXUNUSED(event))
{

    spdlog::info("Lod View menu selected");

    auto& config = config::GetConfig();
    auto& configPath = config.GetAppLogPath();
    if (!std::filesystem::exists(configPath))
    {
        spdlog::error("Config file does not exist: {}", configPath);
        wxMessageBox(
            "Config file does not exist.", "Error", wxOK | wxICON_ERROR);
        return;
    }
    spdlog::info("Opening config file: {}", configPath);
    // Use wxLaunchDefaultApplication to open the config file in the default
    // editor
    wxLaunchDefaultApplication(configPath);
}

void MainWindow::ParseData(const std::string filePath)
{
    spdlog::info("Parsing data from file: {}", filePath);

    // Add your file processing code here
    // For example, you can call your XML parser to parse the selected file
    m_searchResultsList->DeleteAllItems();
    m_searchBox->SetValue("");
    m_events.Clear();
    m_parser = std::make_shared<parser::XmlParser>();

    m_parser->RegisterObserver(this);

    SetStatusText("Loading ..");
    m_progressGauge->SetValue(0);

    m_processing = true;

    m_progressGauge->SetRange(m_parser->GetTotalProgress());
    m_parser->ParseData(filePath);
    SetStatusText("Data ready");
    m_processing = false;

    spdlog::info("Parsing complete.");
}

void MainWindow::UpdateFilters()
{
    // Collect unique values for each filter
    std::set<std::string> types;
    std::set<std::string> timestamps;
    for (unsigned long i = 0; i < m_events.Size(); ++i)
    {
        const auto& event = m_events.GetEvent(i);
        types.insert(event.findByKey("type"));
        timestamps.insert(event.findByKey("timestamp"));
    }

    // Update type filter
    m_typeFilter->Freeze();
    m_typeFilter->Clear();
    m_typeFilter->Append("All");
    for (const auto& t : types)
        m_typeFilter->Append(t);
    m_typeFilter->SetSelection(0);
    m_typeFilter->Thaw();
    m_typeFilter->Show();

    // Update timestamp filter
    m_timestampFilter->Freeze();
    m_timestampFilter->Clear();
    m_timestampFilter->Append("All");
    for (const auto& ts : timestamps)
        m_timestampFilter->Append(ts);
    m_timestampFilter->SetSelection(0);
    m_timestampFilter->Thaw();

    // If you have more filters, update them here in the same way
    // Refresh the left panel layout
    Refresh();
    this->Layout();
}

// Data Observer methods
void MainWindow::ProgressUpdated()
{
    int progress = m_parser->GetCurrentProgress();
    spdlog::debug("Progress updated: {}", progress);
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
    spdlog::debug("New event found.");
    m_events.AddEvent(std::move(event));
}

} // namespace gui