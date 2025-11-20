#include "ui/wx/MainWindow.hpp"
#include "config/Config.hpp"
#include "filters/FilterManager.hpp"
#include "ui/wx/ConfigEditorDialog.hpp"
#include "ui/wx/FiltersPanel.hpp"
#include "ui/wx/ItemVirtualListControl.hpp"
#include "util/WxWidgetsUtils.hpp"
#include "util/Logger.hpp"
#include "mvc/MainController.hpp"
#include <wx/notebook.h>

#include "error/Error.hpp"
#include "ui/wx/EventsVirtualListControl.hpp"

#include <wx/app.h>
#include <wx/settings.h>
#include <wx/statline.h>
// std
#include <filesystem>
#include <wx/utils.h> // wxLaunchDefaultApplication

namespace
{
inline std::filesystem::path WxToPath(const wxString& s)
{
#if defined(_WIN32)
    return std::filesystem::path {s.ToStdWstring()};
#else
    return std::filesystem::path {s.ToStdString()};
#endif
}

} // namespace

namespace ui::wx
{
MainWindow::MainWindow(const wxString& title, const wxPoint& pos,
    const wxSize& size, const Version::Version& version)
    : wxFrame(NULL, wxID_ANY, title, pos, size)
    , m_version(version)
    , m_fileHistory(9) // Initialize file history to remember up to 9 files
{
    util::Logger::Info(
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
    m_applyFilterButton->Bind(wxEVT_BUTTON, &MainWindow::OnFilterChanged, this);

    this->Bind(wxEVT_MENU, &MainWindow::OnThemeLight, this, 2001);
    this->Bind(wxEVT_MENU, &MainWindow::OnThemeDark, this, 2002);

    this->DragAcceptFiles(true);
    this->Bind(wxEVT_DROP_FILES, &MainWindow::OnDropFiles, this);
    this->Bind(
        wxEVT_MENU, &MainWindow::OnReloadConfig, this, ID_ParserReloadConfig);


    // Add this binding for recent files
    this->Bind(
        wxEVT_MENU, &MainWindow::LoadRecentFile, this, wxID_FILE1, wxID_FILE9);

    m_controller = std::make_unique<mvc::MainController>(m_events);

    if (m_searchResultsView && m_controller)
    {
        m_presenter = std::make_unique<ui::MainWindowPresenter>(
            *this, *m_controller, m_events, *m_searchResultsView,
            m_eventsListView, m_typeFilterView, m_itemDetailsView);
    }
    else
    {
        util::Logger::Error(
            "Unable to construct MainWindowPresenter due to missing dependencies");
    }
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

    // Add this section for recent files
    menuFile->AppendSeparator();
    m_fileHistory.UseMenu(menuFile);
    m_fileHistory.AddFilesToMenu();
    menuFile->AppendSeparator();
    menuFile->Append(wxID_EXIT, "E&xit\tAlt-X", "Quit the application");

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

    // NEW: Config menu - move items here
    wxMenu* menuConfig = new wxMenu;
    menuConfig->Append(
        ID_ConfigEditor, "Edit &Config...\tCtrl+E", "Open Config Editor");
    menuConfig->Append(
        ID_AppLogViewer, "Open &Application Logs...\tCtrl+L", "Open App Logs");
    menuConfig->Append(
        ID_ParserReloadConfig, "Reload Config", "Reload configuration file");

    // Main menu bar
    wxMenuBar* menuBar = new wxMenuBar;
    menuBar->Append(menuFile, "&File");
#ifndef __WXMAC__ // Add "About" manually for non-macOS
    menuBar->Append(menuHelp, "&Help");
#endif
    menuBar->Append(menuParser, "&Parser");
    menuBar->Append(menuConfig, "&Config"); // Add Config menu
    menuBar->Append(menuTheme, "&Theme");
    menuBar->Append(menuView, "&View");
    SetMenuBar(menuBar);
}

void MainWindow::OnSize(wxSizeEvent& event)
{

    event.Skip();

    auto* sb = GetStatusBar();
    if (!sb || !m_progressGauge)
        return;
    if (sb->GetFieldsCount() < 2)
        return;

    wxRect rect;
    if (!sb->GetFieldRect(1, rect))
        return; // field 1 hosts the gauge
    if (!rect.IsEmpty())
        m_progressGauge->SetSize(rect);
}

void MainWindow::ApplyDarkThemeRecursive(wxWindow* window)
{
    if (!window)
        return;

    window->SetBackgroundColour(wxColour(0, 0, 0));
    window->SetForegroundColour(wxColour(255, 255, 255));

    for (auto child : window->GetChildren())
    {
        ApplyDarkThemeRecursive(child);
    }
}

void MainWindow::ApplyLightThemeRecursive(wxWindow* window)
{
    if (!window)
        return;

    window->SetBackgroundColour(wxColour(255, 255, 255));
    window->SetForegroundColour(wxColour(0, 0, 0));

    for (auto child : window->GetChildren())
    {
        ApplyLightThemeRecursive(child);
    }
}

void MainWindow::OnThemeLight(wxCommandEvent&)
{
    util::Logger::Info("Switching to Light Theme");
    ApplyLightThemeRecursive(this);
    this->Refresh();
    this->Update();
}

void MainWindow::OnThemeDark(wxCommandEvent&)
{
    util::Logger::Info("Switching to Dark Theme");
    ApplyDarkThemeRecursive(this);
    this->Refresh();
    this->Update();
}

void MainWindow::setupLayout()
{
    util::Logger::Info("Setting up layout...");
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

    m_searchResultsList = new ui::wx::SearchResultsView(m_searchResultPanel);
    m_searchResultsList->SetObserver(this);
    m_searchResultsView = m_searchResultsList;
    m_searchResultsView->BeginUpdate({});
    m_searchResultsView->EndUpdate();

    searchSizer->Add(searchBarSizer, 0, wxEXPAND);
    searchSizer->Add(m_searchResultsList, 1, wxEXPAND | wxALL, 5);

    m_searchResultPanel->SetSizer(searchSizer);

    m_eventsListCtrl =
        new ui::wx::EventsVirtualListControl(m_events, m_rigth_spliter);
    auto* itemView =
        new ui::wx::ItemVirtualListControl(m_events, m_rigth_spliter);
    m_eventsListView = m_eventsListCtrl;
    m_itemDetailsView = itemView;
    m_itemDetailsWindow = itemView;

    m_bottom_spliter->SplitHorizontally(
        m_left_spliter, m_searchResultPanel, -1);
    m_bottom_spliter->SetSashGravity(0.3);

    m_left_spliter->SplitVertically(m_leftPanel, m_rigth_spliter, 1);
    m_left_spliter->SetSashGravity(0.3);

    m_rigth_spliter->SplitVertically(m_eventsListCtrl, m_itemDetailsWindow, -1);
    m_rigth_spliter->SetSashGravity(0.75);

    // In setupLayout() method, replace the filterSizer code with this:

    auto* filterSizer = new wxBoxSizer(wxVERTICAL);

    // Create a notebook for tabs
    wxNotebook* filterNotebook = new wxNotebook(m_leftPanel, wxID_ANY);

    // First tab - Extended Filters
    wxPanel* filtersTab = new wxPanel(filterNotebook);
    wxBoxSizer* filtersTabSizer = new wxBoxSizer(wxVERTICAL);

    // Add FiltersPanel to the first tab
    m_filtersPanel = new FiltersPanel(filtersTab);
    filtersTabSizer->Add(m_filtersPanel, 1, wxEXPAND | wxALL, 5);
    filtersTab->SetSizer(filtersTabSizer);

    // Second tab - Type Filters
    wxPanel* typeFilterTab = new wxPanel(filterNotebook);
    wxBoxSizer* typeFilterSizer = new wxBoxSizer(wxVERTICAL);

    // Add type filter controls to the second tab
    m_typeFilterPanel = new ui::wx::TypeFilterView(typeFilterTab);
    m_typeFilterPanel->SetOnFilterChanged(
        [this]() { HandleTypeFilterChanged(); });
    m_typeFilterView = m_typeFilterPanel;

    // Type filter label
    typeFilterSizer->Add(
        new wxStaticText(typeFilterTab, wxID_ANY, "Type:"), 0, wxALL, 5);

    // Type filter list (expandable)
    typeFilterSizer->Add(m_typeFilterPanel, 1, wxEXPAND | wxALL, 5);

    // Apply Filter Button
    m_applyFilterButton = new wxButton(typeFilterTab, wxID_ANY, "Apply Filter");
    typeFilterSizer->Add(m_applyFilterButton, 0, wxEXPAND | wxALL, 5);

    typeFilterTab->SetSizer(typeFilterSizer);

    // Add tabs to notebook
    filterNotebook->AddPage(filtersTab, "Extended Filters");
    filterNotebook->AddPage(typeFilterTab, "Type Filters");

    // Add notebook to the left panel sizer (taking all available space)
    filterSizer->Add(filterNotebook, 1, wxEXPAND | wxALL, 5);

    m_leftPanel->SetSizer(filterSizer);

    CreateToolBar();
    setupStatusBar();
    util::Logger::Info("Layout setup complete.");
}

void MainWindow::OnSearch(wxCommandEvent& WXUNUSED(event))
{
    if (!m_presenter)
    {
        util::Logger::Error("Search requested before presenter initialization");
        return;
    }

    try
    {
        m_presenter->PerformSearch();
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("Exception in OnSearch: {}", ex.what());
        wxMessageBox(wxString::Format("Exception in search:\n%s", ex.what()),
            "Error", wxOK | wxICON_ERROR);
    }
    catch (...)
    {
        util::Logger::Error("Unknown exception in OnSearch");
        wxMessageBox("Unknown error in search.", "Error", wxOK | wxICON_ERROR);
    }
}

void MainWindow::OnSearchResultActivated(long eventId)
{
    try
    {
        for (size_t i = 0; i < m_events.Size(); ++i)
        {
            if (m_events.GetEvent(wx_utils::to_model_index(i)).getId() == eventId)
            {
                m_events.SetCurrentItem(static_cast<int>(i));
                break;
            }
        }
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error(
            "Exception in OnSearchResultActivated: {}", ex.what());
        wxMessageBox(
            wxString::Format("Exception in search result:\n%s", ex.what()),
            "Error", wxOK | wxICON_ERROR);
    }
    catch (...)
    {
        util::Logger::Error("Unknown exception in OnSearchResultActivated");
        wxMessageBox(
            "Unknown error in search result.", "Error", wxOK | wxICON_ERROR);
    }
}

void MainWindow::setupStatusBar()
{
    util::Logger::Info("Setting up status bar...");

    CreateStatusBar(2);
    // Make field 1 wider for the gauge
    int widths[2] = {-1, 200};
    GetStatusBar()->SetStatusWidths(2, widths);

    wxRect rect;
    GetStatusBar()->GetFieldRect(1, rect);

    m_progressGauge = new wxGauge(GetStatusBar(), wxID_ANY, 1,
        rect.GetPosition(), rect.GetSize(), wxGA_SMOOTH);
    m_progressGauge->SetValue(0);

    util::Logger::Info("Status bar setup complete.");
}
#ifndef NDEBUG
void MainWindow::OnPopulateDummyData(wxCommandEvent& WXUNUSED(event))
{
    util::Logger::Info("Populating dummy data...");
    if (m_searchResultsView)
        m_searchResultsView->Clear();
    m_searchBox->SetValue("");
    m_events.Clear();
    SetStatusText("Loading ..");
    m_progressGauge->SetValue(0);

    m_processing = true;
    for (int i = 0; i < 1000; ++i)
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
        }

        if (m_closerequest)
        {
            m_processing = false;
            this->Destroy();
            return;
        }
    }
    m_progressGauge->SetValue(100);
    SetStatusText("Data ready");
    m_processing = false;
    m_progressGauge->Hide();


    util::Logger::Info("Dummy data population complete.");
}
#endif

void MainWindow::OnExit(wxCommandEvent& WXUNUSED(event))
{
    util::Logger::Info("Exit requested.");
    Close(true);
}

void MainWindow::OnClose(wxCloseEvent& event)
{
    util::Logger::Info("Window close event triggered.");
    if (IsBusy())
    {
        util::Logger::Warn("Close requested while processing. Vetoing close.");
        event.Veto();
        m_closerequest = true;
    }
    else
    {
        util::Logger::Info("Destroying window.");
        this->Destroy();
    }
}

void MainWindow::OnAbout(wxCommandEvent& WXUNUSED(event))
{
    util::Logger::Info("About dialog opened.");
    wxMessageBox("This is simple log viewer which parse logs and display "
                 "in table view.",
        "About" + m_version.asLongStr(), wxOK | wxICON_INFORMATION);
}

void MainWindow::OnClearParser(wxCommandEvent& WXUNUSED(event))
{
    try
    {

        util::Logger::Info("Parser clear triggered.");
        if (m_searchResultsView)
            m_searchResultsView->Clear();
        m_searchBox->SetValue("");
        m_events.Clear();
        m_progressGauge->SetValue(0);
        SetStatusText("Data cleared");
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("Exception in OnClearParser: {}", ex.what());
        wxMessageBox(
            wxString::Format("Exception in clear parser:\n%s", ex.what()),
            "Error", wxOK | wxICON_ERROR);
    }
    catch (...)
    {
        util::Logger::Error("Unknown exception in OnClearParser");
        wxMessageBox(
            "Unknown error in clear parser.", "Error", wxOK | wxICON_ERROR);
    }
}

void MainWindow::OnHideSearchResult(wxCommandEvent& event)
{
    util::Logger::Info("Toggling Search Result Panel: {}",
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
    util::Logger::Info(
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
    util::Logger::Info(
        "Toggling Right Panel: {}", event.IsChecked() ? "Hide" : "Show");
    const bool hide = event.IsChecked();
    if (hide)
    {
        if (m_itemDetailsWindow)
            m_rigth_spliter->Unsplit(m_itemDetailsWindow);
        if (m_presenter)
            m_presenter->SetItemDetailsVisible(false);
    }
    else
    {
        if (m_itemDetailsWindow)
            m_rigth_spliter->SplitVertically(
                m_eventsListCtrl, m_itemDetailsWindow, -1);
        if (m_presenter)
            m_presenter->SetItemDetailsVisible(true);
    }
    this->Layout();
}

void MainWindow::OnOpenFile(wxCommandEvent& WXUNUSED(event))
{
    try
    {
        util::Logger::Info("OnOpenFile event triggered.");

        CallAfter(
            [this]()
            {
                try
                {
                    if (IsBusy())
                    {
                        // Never throw out of the event loop
                        error::Error("A file is already being processed. "
                                     "Please wait.");
                        return;
                    }

                    static wxString lastDir;

                    wxFileDialog openFileDialog(this, _("Open log file"),
                        lastDir, "",
                        "XML files (*.xml)|*.xml|Log files "
                        "(*.log)|*.log|All "
                        "files (*.*)|*.*",
                        wxFD_OPEN | wxFD_FILE_MUST_EXIST);

                    if (openFileDialog.ShowModal() == wxID_OK)
                    {
                        wxString path = openFileDialog.GetPath();
                        lastDir = openFileDialog.GetDirectory();

                        AddToRecentFiles(path);
                        auto filePathObj = WxToPath(path);
                        ProcessFileSelection(filePathObj);
                    }
                    else
                    {
                        util::Logger::Warn("File dialog was canceled by the user.");
                    }
                }
                catch (const error::Error& e)
                {
                    util::Logger::Error(
                        "Application error in OnOpenFile (UI): {}", e.what());
                }
                catch (const std::exception& e)
                {
                    util::Logger::Error("Exception in OnOpenFile (UI): {}", e.what());
                    wxMessageBox(wxString::Format(
                                     "Exception in open file:\n%s", e.what()),
                        "Error", wxOK | wxICON_ERROR);
                }
                catch (...)
                {
                    util::Logger::Error("Unknown exception in OnOpenFile (UI)");
                    wxMessageBox("Unknown error in open file.", "Error",
                        wxOK | wxICON_ERROR);
                }
            });
    }
    catch (const error::Error& err)
    {
        util::Logger::Error("Application error in OnOpenFile: {}", err.what());
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("Exception in OnOpenFile: {}", ex.what());
        wxMessageBox(wxString::Format("Exception in open file:\n%s", ex.what()),
            "Error", wxOK | wxICON_ERROR);
    }
    catch (...)
    {
        util::Logger::Error("Unknown exception in OnOpenFile");
        wxMessageBox(
            "Unknown error in open file.", "Error", wxOK | wxICON_ERROR);
    }
}

void MainWindow::OnOpenConfigEditor(wxCommandEvent& WXUNUSED(event))
{
    ConfigEditorDialog dialog(this);

    // Register as observer before showing dialog
    dialog.AddObserver(this);

    dialog.ShowModal();
    // Observer is automatically removed when dialog is destroyed
}

// Observer interface implementation
void MainWindow::OnConfigChanged()
{
    util::Logger::Info("Configuration changed, updating UI...");
    // Update any UI elements that depend on config
    if (m_eventsListView)
    {
        m_eventsListView->RefreshColumns();
        m_eventsListView->RefreshView();
    }

    // Force layout update if needed
    Layout();
}
void MainWindow::OnOpenAppLog(wxCommandEvent& WXUNUSED(event))
{
    try
    {
        util::Logger::Info("Lod View menu selected");

        auto& config = config::GetConfig();
        auto& configPath = config.GetAppLogPath();
        if (!std::filesystem::exists(configPath))
        {
            util::Logger::Error("Config file does not exist: {}", configPath);
            wxMessageBox(
                "Config file does not exist.", "Error", wxOK | wxICON_ERROR);
            return;
        }
        util::Logger::Info("Opening config file: {}", configPath);
        wxLaunchDefaultApplication(configPath);
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("Exception in OnOpenAppLog: {}", ex.what());
        wxMessageBox(wxString::Format("Exception in app log:\n%s", ex.what()),
            "Error", wxOK | wxICON_ERROR);
    }
    catch (...)
    {
        util::Logger::Error("Unknown exception in OnOpenAppLog");
        wxMessageBox("Unknown error in app log.", "Error", wxOK | wxICON_ERROR);
    }
}


void MainWindow::AddToRecentFiles(const wxString& path)
{
    m_fileHistory.AddFileToHistory(path);
}

void MainWindow::ProcessFileSelection(const std::filesystem::path& filePath)
{
    if (!m_presenter)
    {
        throw error::Error("Presenter not initialized.");
    }

    m_presenter->LoadLogFile(filePath);
    m_presenter->SetItemDetailsVisible(true);
}

bool MainWindow::IsBusy() const
{
    const bool presenterBusy = m_presenter && m_presenter->IsParsing();
    return m_processing.load() || presenterBusy;
}

void MainWindow::OnDropFiles(wxDropFilesEvent& event)
{
    try
    {
        if (event.GetNumberOfFiles() > 0)
        {
            wxString* droppedFiles = event.GetFiles();
            wxString filename = droppedFiles[0];
            if (filename.IsEmpty())
            {
                throw error::Error("Dropped file path is empty.");
            }
            auto filePathObj = WxToPath(filename);
            util::Logger::Info("File dropped: {}", filePathObj.string());

            // Process the dropped file
            ProcessFileSelection(filePathObj);

            // Add to recent files
            AddToRecentFiles(filename);
        }
    }
    catch (const error::Error& err)
    {
        util::Logger::Error("Application error in OnDropFiles: {}", err.what());
        // Message box already shown by error::Error
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("Exception in OnDropFiles: {}", ex.what());
        wxMessageBox(wxString::Format("Exception in file drop:\n%s", ex.what()),
            "Error", wxOK | wxICON_ERROR);
    }
    catch (...)
    {
        util::Logger::Error("Unknown exception in OnDropFiles");
        wxMessageBox(
            "Unknown error in file drop.", "Error", wxOK | wxICON_ERROR);
    }
    util::Logger::Info("File drop processing complete.");
}

void MainWindow::LoadRecentFile(wxCommandEvent& event)
{
    int fileId = event.GetId() - wxID_FILE1;

    try
    {
        if (m_presenter)
            m_presenter->SetItemDetailsVisible(true);

        if (fileId < 0 ||
            static_cast<size_t>(fileId) >= m_fileHistory.GetCount())
        {
            throw error::Error("Invalid recent file index.");
        }

        wxString path =
            m_fileHistory.GetHistoryFile(static_cast<size_t>(fileId));
        if (path.IsEmpty() || !wxFileExists(path))
        {
            util::Logger::Warn("Recent file not found: {}", path.ToStdString());
            m_fileHistory.RemoveFileFromHistory(static_cast<size_t>(fileId));
            throw error::Error(
                std::string("Recent file not found: ") + path.ToStdString());
        }

        auto filePathObj = WxToPath(path);
        ProcessFileSelection(filePathObj);
    }
    catch (const error::Error& err)
    {
        util::Logger::Error("Application error in LoadRecentFile: {}", err.what());
        // Message box already shown by error::Error
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("Exception in LoadRecentFile: {}", ex.what());
        wxMessageBox(
            wxString::Format("Exception in load recent file:\n%s", ex.what()),
            "Error", wxOK | wxICON_ERROR);
    }
    catch (...)
    {
        util::Logger::Error("Unknown exception in LoadRecentFile");
        wxMessageBox(
            "Unknown error in load recent file.", "Error", wxOK | wxICON_ERROR);
    }
}
void MainWindow::OnReloadConfig(wxCommandEvent&)
{
    try
    {
        util::Logger::Info("Reloading configuration...");
        config::GetConfig().Reload();
        if (m_eventsListView)
        {
            m_eventsListView->UpdateColors();
        }
        util::Logger::Info("Configuration reload complete.");
        wxMessageBox(
            "Configuration reloaded.", "Info", wxOK | wxICON_INFORMATION);
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("Exception during configuration reload: {}", ex.what());
        wxMessageBox(
            wxString::Format(
                "Exception during configuration reload:\n%s", ex.what()),
            "Error", wxOK | wxICON_ERROR);
    }
    catch (...)
    {
        util::Logger::Error("Unknown exception during configuration reload.");
        wxMessageBox("Unknown error during configuration reload.", "Error",
            wxOK | wxICON_ERROR);
    }
}

void MainWindow::OnFilterChanged(wxCommandEvent&)
{
    if (m_presenter)
        m_presenter->ApplySelectedTypeFilters();
}

void MainWindow::HandleTypeFilterChanged()
{
    if (m_presenter)
        m_presenter->ApplySelectedTypeFilters();
}

void MainWindow::ApplyFilters()
{
    if (!m_eventsListView || m_events.Size() == 0)
    {
        return;
    }

    wxString statusTxt =
        GetStatusBar() ? GetStatusBar()->GetStatusText(0) : wxString();
    SetStatusText("Applying filters...");

    if (!m_eventsListView)
    {
        util::Logger::Warn("Events list view not initialized; cannot apply saved filters.");
        SetStatusText(statusTxt);
        return;
    }

    // Apply filters from FilterManager to the events container
    auto filteredIndices =
        filters::FilterManager::getInstance().applyFilters(m_events);

    m_eventsListView->SetFilteredEvents(filteredIndices);
    m_eventsListView->RefreshView();

    SetStatusText(statusTxt);
}

std::string MainWindow::ReadSearchQuery() const
{
    return m_searchBox ? m_searchBox->GetValue().ToStdString() : std::string {};
}

std::string MainWindow::CurrentStatusText() const
{
    if (auto* status = GetStatusBar())
        return status->GetStatusText(0).ToStdString();
    return {};
}

void MainWindow::UpdateStatusText(const std::string& text)
{
    SetStatusText(wxString::FromUTF8(text.c_str()));
}

void MainWindow::SetSearchControlsEnabled(bool enabled)
{
    if (m_searchButton)
        m_searchButton->Enable(enabled);
}

void MainWindow::ToggleProgressVisibility(bool visible)
{
    if (!m_progressGauge)
        return;

    if (visible)
    {
        m_progressGauge->Show();
        m_progressGauge->Refresh();
        m_progressGauge->Update();
    }
    else
    {
        m_progressGauge->Hide();
    }
}

void MainWindow::ConfigureProgressRange(int range)
{
    if (m_progressGauge)
        m_progressGauge->SetRange(range);
}

void MainWindow::UpdateProgressValue(int value)
{
    if (m_progressGauge)
        m_progressGauge->SetValue(value);
}

void MainWindow::ProcessPendingEvents()
{
    wxSafeYield(this, true);
}

void MainWindow::RefreshLayout()
{
    Refresh();
    Layout();
}

} // namespace ui::wx