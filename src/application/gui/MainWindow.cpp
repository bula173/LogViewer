#include "gui/MainWindow.hpp"
#include "config/Config.hpp"
#include "filters/FilterManager.hpp"
#include "gui/ConfigEditorDialog.hpp"
#include "gui/FiltersPanel.hpp"
#include <wx/notebook.h>

#include "error/Error.hpp"
#include "gui/EventsVirtualListControl.hpp"

#include <spdlog/spdlog.h>
#include <wx/app.h>
#include <wx/settings.h>
#include <wx/statline.h>
// std
#include <filesystem>
#include <set>
#include <unordered_set>
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

// Helper: get columns to show in search results from config.
// Adjust the accessor to match your Config API (e.g. GetSearchColumns(),
// searchResultColumns, etc.)
inline std::vector<std::string> GetSearchColumnsFromConfig()
{
    // TODO: If your config uses a different name, change this line accordingly:
    // Example options you might have in your project:
    // return cfg.searchResultColumns;
    // return cfg.visibleColumns;
    // return cfg.GetSearchColumns();
    // Fallback defaults if not available:
    return std::vector<std::string> {"timestamp", "type", "info"};
}

// Rebuild the search results columns: [Event ID] [Match] + configured columns
// (unique)
inline void SetupSearchResultColumns(wxDataViewListCtrl* list)
{
    if (!list)
        return;

    list->ClearColumns();

    // Base columns
    list->AppendTextColumn("Event ID");
    list->AppendTextColumn("Match");

    // Config-driven columns
    std::unordered_set<std::string> added;
    added.insert("event id");
    added.insert("match");

    std::vector<std::string> cfgCols = GetSearchColumnsFromConfig();
    for (auto& c : cfgCols)
    {
        if (c.empty())
            continue;
        std::string key = c;
        for (auto& ch : key)
            ch = static_cast<char>(::tolower(static_cast<unsigned char>(ch)));
        if (!added.count(key))
        {
            list->AppendTextColumn(wxString::FromUTF8(c.c_str()));
            added.insert(key);
        }
    }
}
} // namespace

namespace gui
{
MainWindow::MainWindow(const wxString& title, const wxPoint& pos,
    const wxSize& size, const Version::Version& version)
    : wxFrame(NULL, wxID_ANY, title, pos, size)
    , m_version(version)
    , m_fileHistory(9) // Initialize file history to remember up to 9 files
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
    window->SetForegroundColour(wxColour(10, 10, 10));

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
    m_typeFilter = new wxCheckListBox(
        typeFilterTab, wxID_ANY, wxDefaultPosition, wxDefaultSize);
    m_typeFilter->Bind(
        wxEVT_CONTEXT_MENU, &MainWindow::OnTypeFilterContextMenu, this);

    // Type filter label
    typeFilterSizer->Add(
        new wxStaticText(typeFilterTab, wxID_ANY, "Type:"), 0, wxALL, 5);

    // Type filter list (expandable)
    typeFilterSizer->Add(m_typeFilter, 1, wxEXPAND | wxALL, 5);

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
    spdlog::info("Layout setup complete.");
}

void MainWindow::OnSearch(wxCommandEvent& WXUNUSED(event))
{
    try
    {
        wxString query = m_searchBox->GetValue();
        wxString statusTxt =
            GetStatusBar() ? GetStatusBar()->GetStatusText(0) : wxString();
        const std::string queryUtf8 = query.ToStdString();

        // Freeze to avoid re-entrancy during column rebuild and row appends
        m_searchResultsList->Freeze();

        // Clear old rows and rebuild columns to reflect current config
        m_searchResultsList->DeleteAllItems();
        SetupSearchResultColumns(m_searchResultsList);

        // Prepare configured columns (must match SetupSearchResultColumns
        // order)
        const std::vector<std::string> cfgCols = GetSearchColumnsFromConfig();

        const unsigned long total = m_events.Size();

        // Prepare progress bar
        if (m_progressGauge)
        {
            m_progressGauge->Show();
            m_progressGauge->SetRange(
                static_cast<int>(std::max<unsigned long>(total, 1)));
            m_progressGauge->SetValue(0);
        }
        SetStatusText("Searching ...");
        if (m_searchButton)
            m_searchButton->Enable(false);


        for (unsigned long i = 0; i < total; ++i)
        {
            const auto& event = m_events.GetEvent(i);

            // Query match
            std::string matchedText;
            if (!queryUtf8.empty())
            {
                for (const auto& item : event.getEventItems())
                {
                    if (item.second.find(queryUtf8) != std::string::npos)
                    {
                        matchedText = item.second;
                        break;
                    }
                }
                if (matchedText.empty())
                {
                    if (m_progressGauge)
                        m_progressGauge->SetValue(static_cast<int>(i + 1));

                    // Yield occasionally to keep UI responsive
                    if ((i % 500) == 0)
                        wxSafeYield(this, true);
                    continue;
                }
            }

            // Row: [Event ID] [Match] + configured columns
            wxVector<wxVariant> row;
            row.push_back(wxVariant(std::to_string(event.getId())));
            row.push_back(wxVariant(wxString::FromUTF8(matchedText.c_str())));

            for (const auto& key : cfgCols)
            {
                const std::string value = event.findByKey(key);
                row.push_back(wxVariant(wxString::FromUTF8(value.c_str())));
            }

            m_searchResultsList->AppendItem(row);

            if (m_progressGauge)
                m_progressGauge->SetValue(static_cast<int>(i + 1));

            // Yield occasionally to keep UI responsive
            if ((i % 500) == 0)
                wxSafeYield(this, true);
        }

        // Finish
        if (m_progressGauge)
        {
            m_progressGauge->SetValue(
                static_cast<int>(std::max<unsigned long>(total, 1)));
            m_progressGauge->Hide();
        }
        if (m_searchButton)
            m_searchButton->Enable(true);
        SetStatusText(statusTxt);

        m_searchResultsList->Thaw(); // unfreeze after the list is fully built
        wxSafeYield(this, true);     // let paints/layouts catch up
    }
    catch (const std::exception& ex)
    {
        if (m_searchResultsList)
            m_searchResultsList->Thaw();
        spdlog::error("Exception in OnSearch: {}", ex.what());
        wxMessageBox(wxString::Format("Exception in search:\n%s", ex.what()),
            "Error", wxOK | wxICON_ERROR);
        if (m_progressGauge)
            m_progressGauge->Hide();
        if (m_searchButton)
            m_searchButton->Enable(true);
    }
    catch (...)
    {
        if (m_searchResultsList)
            m_searchResultsList->Thaw();
        spdlog::error("Unknown exception in OnSearch");
        wxMessageBox("Unknown error in search.", "Error", wxOK | wxICON_ERROR);
        if (m_progressGauge)
            m_progressGauge->Hide();
        if (m_searchButton)
            m_searchButton->Enable(true);
    }
}

void MainWindow::OnSearchResultActivated(wxDataViewEvent& event)
{
    try
    {
        int itemIndex = m_searchResultsList->ItemToRow(event.GetItem());
        if (itemIndex == wxNOT_FOUND)
        {
            spdlog::warn("OnSearchResultActivated: invalid row");
            return;
        }
        if (m_searchResultsList->GetColumnCount() == 0)
        {
            spdlog::warn("OnSearchResultActivated: no columns");
            return;
        }

        wxVariant value;
        m_searchResultsList->GetValue(
            value, itemIndex, 0); // column 0 = Event ID
        wxString eventIdStr = value.GetString();
        long eventId;
        if (!eventIdStr.ToLong(&eventId))
        {
            spdlog::warn(
                "Could not convert event ID: {}", eventIdStr.ToStdString());
            return;
        }

        for (unsigned long i = 0; i < m_events.Size(); ++i)
        {
            if (m_events.GetEvent(i).getId() == eventId)
            {
                m_events.SetCurrentItem(i);
                break;
            }
        }
    }
    catch (const std::exception& ex)
    {
        spdlog::error("Exception in OnSearchResultActivated: {}", ex.what());
        wxMessageBox(
            wxString::Format("Exception in search result:\n%s", ex.what()),
            "Error", wxOK | wxICON_ERROR);
    }
    catch (...)
    {
        spdlog::error("Unknown exception in OnSearchResultActivated");
        wxMessageBox(
            "Unknown error in search result.", "Error", wxOK | wxICON_ERROR);
    }
}

void MainWindow::setupStatusBar()
{
    spdlog::info("Setting up status bar...");

    CreateStatusBar(2);
    // Make field 1 wider for the gauge
    int widths[2] = {-1, 200};
    GetStatusBar()->SetStatusWidths(2, widths);

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
    try
    {

        spdlog::info("Parser clear triggered.");
        m_searchResultsList->DeleteAllItems();
        m_searchBox->SetValue("");
        m_events.Clear();
        m_progressGauge->SetValue(0);
        SetStatusText("Data cleared");
    }
    catch (const std::exception& ex)
    {
        spdlog::error("Exception in OnClearParser: {}", ex.what());
        wxMessageBox(
            wxString::Format("Exception in clear parser:\n%s", ex.what()),
            "Error", wxOK | wxICON_ERROR);
    }
    catch (...)
    {
        spdlog::error("Unknown exception in OnClearParser");
        wxMessageBox(
            "Unknown error in clear parser.", "Error", wxOK | wxICON_ERROR);
    }
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
    try
    {
        spdlog::info("OnOpenFile event triggered.");

        CallAfter(
            [this]()
            {
                try
                {
                    if (m_processing)
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
                        ParseData(filePathObj);
                        UpdateFilters();
                    }
                    else
                    {
                        spdlog::warn("File dialog was canceled by the user.");
                    }
                }
                catch (const error::Error& e)
                {
                    spdlog::error(
                        "Application error in OnOpenFile (UI): {}", e.what());
                }
                catch (const std::exception& e)
                {
                    spdlog::error("Exception in OnOpenFile (UI): {}", e.what());
                    wxMessageBox(wxString::Format(
                                     "Exception in open file:\n%s", e.what()),
                        "Error", wxOK | wxICON_ERROR);
                }
                catch (...)
                {
                    spdlog::error("Unknown exception in OnOpenFile (UI)");
                    wxMessageBox("Unknown error in open file.", "Error",
                        wxOK | wxICON_ERROR);
                }
            });
    }
    catch (const error::Error& err)
    {
        spdlog::error("Application error in OnOpenFile: {}", err.what());
    }
    catch (const std::exception& ex)
    {
        spdlog::error("Exception in OnOpenFile: {}", ex.what());
        wxMessageBox(wxString::Format("Exception in open file:\n%s", ex.what()),
            "Error", wxOK | wxICON_ERROR);
    }
    catch (...)
    {
        spdlog::error("Unknown exception in OnOpenFile");
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
    spdlog::info("Configuration changed, updating UI...");
    // Update any UI elements that depend on config
    if (m_eventsListCtrl)
    {
        m_eventsListCtrl->RefreshColumns(); // Assuming this method exists
        m_eventsListCtrl->Refresh();
    }

    // Force layout update if needed
    Layout();
}
void MainWindow::OnOpenAppLog(wxCommandEvent& WXUNUSED(event))
{
    try
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
        wxLaunchDefaultApplication(configPath);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("Exception in OnOpenAppLog: {}", ex.what());
        wxMessageBox(wxString::Format("Exception in app log:\n%s", ex.what()),
            "Error", wxOK | wxICON_ERROR);
    }
    catch (...)
    {
        spdlog::error("Unknown exception in OnOpenAppLog");
        wxMessageBox("Unknown error in app log.", "Error", wxOK | wxICON_ERROR);
    }
}

void MainWindow::ParseData(const std::filesystem::path& filePath)
{
    try
    {
        spdlog::info("Parsing data from file: {}", filePath.string());

        if (filePath.empty())
        {
            throw error::Error("File path is empty.");
        }
        if (!std::filesystem::exists(filePath))
        {
            throw error::Error("File does not exist: " + filePath.string());
        }
        m_searchResultsList->DeleteAllItems();
        m_searchBox->SetValue("");
        m_events.Clear();
        m_parser = std::make_shared<parser::XmlParser>();

        m_parser->RegisterObserver(this);

        SetStatusText("Loading ..");
        m_progressGauge->Show();
        m_progressGauge->SetRange(100);
        m_progressGauge->SetValue(0);
        m_progressGauge->Refresh();
        m_progressGauge->Update();

        m_processing = true;

        m_progressGauge->SetRange(m_parser->GetTotalProgress());
        m_parser->ParseData(filePath);
        m_progressGauge->SetValue(100);
        SetStatusText("Data ready. Path: " + filePath.string());
        m_processing = false;
        m_progressGauge->Hide();

        spdlog::info("Parsing complete.");
    }
    catch (const error::Error& err)
    {
        spdlog::error("Application error in ParseData: {}", err.what());
        m_processing = false;
        m_progressGauge->Hide();
        // Message box already shown by error::Error
    }
    catch (const std::exception& ex)
    {
        spdlog::error("Exception in ParseData: {}", ex.what());
        wxMessageBox(
            wxString::Format("Exception during parsing:\n%s", ex.what()),
            "Error", wxOK | wxICON_ERROR);
        m_processing = false;
        m_progressGauge->Hide();
    }
    catch (...)
    {
        spdlog::error("Unknown exception in ParseData");
        wxMessageBox(
            "Unknown error during parsing.", "Error", wxOK | wxICON_ERROR);
        m_processing = false;
        m_progressGauge->Hide();
    }
}

void MainWindow::UpdateFilters()
{

    try
    {
        std::set<std::string> types;
        wxString statusTxt =
            GetStatusBar() ? GetStatusBar()->GetStatusText(0) : wxString();
        SetStatusText("Updating filters...");

        for (unsigned long i = 0; i < m_events.Size(); ++i)
        {
            const auto& event = m_events.GetEvent(i);
            types.insert(event.findByKey("type"));
            if ((i % 1000) == 0)
                wxSafeYield(this, true);
        }

        m_typeFilter->Clear();
        for (const auto& t : types)
        {
            m_typeFilter->Append(t);
            m_typeFilter->Check(m_typeFilter->GetCount() - 1, true);
        }
        m_typeFilter->Show();

        Refresh();
        this->Layout();
        SetStatusText(statusTxt);
    }
    catch (const error::Error& e)
    {
        spdlog::error("Application error in UpdateFilters: {}", e.what());
        throw; // propagate
    }
    catch (const std::exception& e)
    {
        throw error::Error(std::string("UpdateFilters failed: ") + e.what());
    }
}

// Data Observer methods
void MainWindow::ProgressUpdated()
{
    if (!m_parser || !m_progressGauge)
        return;

    int progress = 0;
    try
    {
        progress = m_parser->GetCurrentProgress();
    }
    catch (const std::exception& e)
    {
        spdlog::error(
            "ProgressUpdated: exception getting progress: {}", e.what());
        return;
    }

    spdlog::debug("Progress updated: {}", progress);
    m_progressGauge->SetValue(progress);
    wxYieldIfNeeded(); // safer than wxYield()

    if (progress >= m_progressGauge->GetRange())
        m_processing = false;

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

void MainWindow::NewEventBatchFound(
    std::vector<std::pair<int, db::LogEvent::EventItems>>&& eventBatch)
{
    spdlog::debug("New event batch found with size: {}", eventBatch.size());
    m_events.AddEventBatch(std::move(eventBatch));
    if (m_eventsListCtrl)
        m_eventsListCtrl->Refresh();
    if (m_itemView)
        m_itemView->Refresh();
}

void MainWindow::AddToRecentFiles(const wxString& path)
{
    m_fileHistory.AddFileToHistory(path);
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
            spdlog::info("File dropped: {}", filePathObj.string());

            // Process the dropped file
            ParseData(filePathObj);
            UpdateFilters();

            // Add to recent files
            AddToRecentFiles(filename);
        }
    }
    catch (const error::Error& err)
    {
        spdlog::error("Application error in OnDropFiles: {}", err.what());
        // Message box already shown by error::Error
    }
    catch (const std::exception& ex)
    {
        spdlog::error("Exception in OnDropFiles: {}", ex.what());
        wxMessageBox(wxString::Format("Exception in file drop:\n%s", ex.what()),
            "Error", wxOK | wxICON_ERROR);
    }
    catch (...)
    {
        spdlog::error("Unknown exception in OnDropFiles");
        wxMessageBox(
            "Unknown error in file drop.", "Error", wxOK | wxICON_ERROR);
    }
    spdlog::info("File drop processing complete.");
}

void MainWindow::LoadRecentFile(wxCommandEvent& event)
{
    int fileId = event.GetId() - wxID_FILE1;

    try
    {
        if (fileId < 0 ||
            static_cast<size_t>(fileId) >= m_fileHistory.GetCount())
        {
            throw error::Error("Invalid recent file index.");
        }
        wxString path = m_fileHistory.GetHistoryFile(fileId);
        if (path.IsEmpty() || !wxFileExists(path))
        {
            spdlog::warn("Recent file not found: {}", path.ToStdString());
            m_fileHistory.RemoveFileFromHistory(fileId);
            throw error::Error(
                std::string("Recent file not found: ") + path.ToStdString());
        }
        auto filePathObj = WxToPath(path);
        ParseData(filePathObj);
        UpdateFilters();
    }
    catch (const error::Error& err)
    {
        spdlog::error("Application error in LoadRecentFile: {}", err.what());
        // Message box already shown by error::Error
    }
    catch (const std::exception& ex)
    {
        spdlog::error("Exception in LoadRecentFile: {}", ex.what());
        wxMessageBox(
            wxString::Format("Exception in load recent file:\n%s", ex.what()),
            "Error", wxOK | wxICON_ERROR);
    }
    catch (...)
    {
        spdlog::error("Unknown exception in LoadRecentFile");
        wxMessageBox(
            "Unknown error in load recent file.", "Error", wxOK | wxICON_ERROR);
    }
}
void MainWindow::OnTypeFilterContextMenu(wxContextMenuEvent& /*event*/)
{
    wxMenu menu;
    menu.Append(ID_TypeFilter_SelectAll, "Select All");
    menu.Append(ID_TypeFilter_DeselectAll, "Deselect All");
    menu.Append(ID_TypeFilter_InvertSelection, "Invert Selection");
    menu.Bind(wxEVT_COMMAND_MENU_SELECTED, &MainWindow::OnTypeFilterMenu, this);
    PopupMenu(&menu);
}

void MainWindow::OnTypeFilterMenu(wxCommandEvent& event)
{
    if (!m_typeFilter)
        return;
    int count = m_typeFilter->GetCount();
    switch (event.GetId())
    {
        case ID_TypeFilter_SelectAll:
            for (int i = 0; i < count; ++i)
                m_typeFilter->Check(i, true);
            break;
        case ID_TypeFilter_DeselectAll:
            for (int i = 0; i < count; ++i)
                m_typeFilter->Check(i, false);
            break;
        case ID_TypeFilter_InvertSelection:
            for (int i = 0; i < count; ++i)
                m_typeFilter->Check(i, !m_typeFilter->IsChecked(i));
            break;
    }
}

void MainWindow::OnReloadConfig(wxCommandEvent&)
{
    try
    {
        spdlog::info("Reloading configuration...");
        config::GetConfig().Reload();
        m_eventsListCtrl->UpdateColors();
        spdlog::info("Configuration reload complete.");
        wxMessageBox(
            "Configuration reloaded.", "Info", wxOK | wxICON_INFORMATION);
    }
    catch (const std::exception& ex)
    {
        spdlog::error("Exception during configuration reload: {}", ex.what());
        wxMessageBox(
            wxString::Format(
                "Exception during configuration reload:\n%s", ex.what()),
            "Error", wxOK | wxICON_ERROR);
    }
    catch (...)
    {
        spdlog::error("Unknown exception during configuration reload.");
        wxMessageBox("Unknown error during configuration reload.", "Error",
            wxOK | wxICON_ERROR);
    }
}

void MainWindow::OnFilterChanged(wxCommandEvent&)
{
    wxArrayInt selectedType;

    wxString statusTxt =
        GetStatusBar() ? GetStatusBar()->GetStatusText(0) : wxString();
    SetStatusText("Applying filters...");

    if (m_typeFilter == nullptr)
        return;

    m_typeFilter->GetCheckedItems(selectedType);

    std::set<std::string> selectedTypeStrings;
    for (auto idx : selectedType)
        selectedTypeStrings.insert(m_typeFilter->GetString(idx).ToStdString());

    std::vector<unsigned long> filteredIndices;
    for (unsigned long i = 0; i < m_events.Size(); ++i)
    {
        const auto& event = m_events.GetEvent(i);
        std::string eventType = event.findByKey("type");
        bool typeMatch = selectedTypeStrings.empty() ||
            selectedTypeStrings.count(eventType) > 0;

        if (typeMatch)
            filteredIndices.push_back(i);

        if ((i % 1000) == 0)
            wxSafeYield(this, true);
    }

    if (filteredIndices.empty())
    {
        spdlog::info("No events match the selected filters.");
        m_eventsListCtrl->SetFilteredEvents({});
        m_eventsListCtrl->Refresh();
    }
    else
    {
        spdlog::info("Filtered events count: {}", filteredIndices.size());
        m_eventsListCtrl->SetFilteredEvents(filteredIndices);
        m_eventsListCtrl->Refresh();
    }

    SetStatusText(statusTxt);
}

void MainWindow::ApplyFilters()
{
    if (!m_eventsListCtrl || m_events.Size() == 0)
    {
        return;
    }

    wxString statusTxt =
        GetStatusBar() ? GetStatusBar()->GetStatusText(0) : wxString();
    SetStatusText("Applying filters...");

    // Apply filters from FilterManager to the events container
    auto filteredIndices =
        filters::FilterManager::getInstance().applyFilters(m_events);

    // Update the events list with filtered indices
    auto adapter =
        static_cast<gui::EventsContainerAdapter*>(m_eventsListCtrl->GetModel());
    if (adapter)
    {
        adapter->SetFilteredIndices(filteredIndices);
        m_eventsListCtrl->Refresh();
    }

    SetStatusText(statusTxt);
}

} // namespace gui