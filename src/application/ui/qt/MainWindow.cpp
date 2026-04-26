#include "MainWindow.hpp"

#include "EventsContainer.hpp"
#include "ExportManager.hpp"
#include "FilterManager.hpp"
#include "StatsSummaryPanel.hpp"
#include "ActorsPanel.hpp"
#include "ActorDefinitionsPanel.hpp"
#include "UpdateChecker.hpp"
#include "UpdateDialog.hpp"
#include "IControler.hpp"
#include "MainWindowPresenter.hpp"
#include "EventsTableView.hpp"
#include "FiltersPanel.hpp"
#include "ItemDetailsView.hpp"
#include "SearchResultsView.hpp"
#include "TypeFilterView.hpp"
#include "LogFileLoadDialog.hpp"
#include "Logger.hpp"
#include "Config.hpp"
#include "ConfigEditorDialog.hpp"
#include "StructuredConfigDialog.hpp"
#include "Version.hpp"
#include "PluginManager.hpp"
#include "ThemeSwitcher.hpp"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QString>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QTimer>
#include <QKeySequence>
#include <QDockWidget>

#include <set>

#include <nlohmann/json.hpp>

#include <filesystem>

#ifdef _WIN32
#include <windows.h>
// Undefine Windows API macros that conflict with our method names
#undef CreateService
#endif
#include <stdexcept>

namespace ui::qt
{

// C-style EventsContainer bridge for plugins
#include "../../../plugin_api/PluginEventsC.h"
#include "../../../plugin_api/PluginHostUiC.h"

static int PluginEvents_GetSizeBridge(void* handle)
{
    if (!handle) return 0;
    auto container = static_cast<db::EventsContainer*>(handle);
    return static_cast<int>(container->Size());
}

static char* PluginEvents_GetEventJsonBridge(void* handle, int index)
{
    if (!handle) return nullptr;
    auto container = static_cast<db::EventsContainer*>(handle);
    try {
        const auto& event = container->GetEvent(index);
        nlohmann::json j;
        for (const auto& kv : event.getEventItems()) {
            // If duplicate keys exist, last one wins
            j[kv.first] = kv.second;
        }
        std::string s = j.dump();
        char* out = (char*)std::malloc(s.size() + 1);
        if (!out) return nullptr;
        memcpy(out, s.c_str(), s.size() + 1);
        return out;
    } catch (...) {
        return nullptr;
    }
}

static void PluginHostUi_SetCurrentItemBridge(void* hostOpaque, int itemIndex)
{
    if (!hostOpaque) return;
    auto container = static_cast<db::EventsContainer*>(hostOpaque);
    util::Logger::Debug("[PluginHostUi] setCurrentItem requested index={}", itemIndex);
    try { container->SetCurrentItem(itemIndex); } catch (...) {}
}


MainWindow::MainWindow(mvc::IController& controller,
    db::EventsContainer& events, QWidget* parent)
    : QMainWindow(parent)
{
    m_events = &events;
    util::Logger::Info("[MainWindow] Initializing main window");

    try {
        LoadRecentFiles();  // Load recent files early
        InitializeUi(events);
        util::Logger::Debug("[MainWindow] UI initialization completed");

        SetupMenus();
        util::Logger::Debug("[MainWindow] Menu setup completed");

        InitializePresenter(controller, events);
        util::Logger::Debug("[MainWindow] Presenter initialization completed");

        // Setup plugin manager with callback
        setupPluginManager();
        util::Logger::Debug("[MainWindow] Plugin manager setup completed");

        // Load plugins and create plugin tabs
        loadPlugins();
        util::Logger::Debug("[MainWindow] Plugin initialization completed");

        util::Logger::Info("[MainWindow] Main window initialized successfully");

        // Restore window layout disabled due to crashes with corrupted/legacy settings
        QSettings settings("LogViewer", "LogViewer");
        const QByteArray geom = settings.value("windowGeometry").toByteArray();
        if (!geom.isEmpty()) {
            restoreGeometry(geom);
        }
        const QByteArray state = settings.value("windowState").toByteArray();
        if (!state.isEmpty()) {
            restoreState(state);
        }
        
        // Force plugin left/config dock to be properly positioned after state restoration
        if (m_pluginLeftDock) {
            m_pluginLeftDock->setFloating(false);
            // Ensure it's in the left dock area
            if (!dockWidgetArea(m_pluginLeftDock)) {
                addDockWidget(Qt::LeftDockWidgetArea, m_pluginLeftDock);
            }
            // Tab with filters
            tabifyDockWidget(m_filtersDock, m_pluginLeftDock);
        }

    } catch (const std::exception& ex) {
        util::Logger::Error("[MainWindow] Initialization failed: {}", ex.what());
        throw; // Re-throw to let the application handle it
    }
}

MainWindow::~MainWindow()
{
    // Save recent files before cleanup
    SaveRecentFiles();
    
    // Clean up presenter first to ensure proper disconnection before Qt widgets are destroyed
    m_presenter.reset();

    // Save window layout
    QSettings settings("LogViewer", "LogViewer");
    settings.setValue("windowGeometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void MainWindow::InitializeUi(db::EventsContainer& events)
{
    util::Logger::Debug("[MainWindow] InitializeUi: events size={}",
        events.Size());

    try {
        // ===== STATUS BAR =====
        m_statusLabel = new QLabel("Ready", this);
        if (!m_statusLabel) {
            throw std::runtime_error("Failed to create status label");
        }
        statusBar()->addWidget(m_statusLabel, 1);

        m_progressBar = new QProgressBar(this);
        if (!m_progressBar) {
            throw std::runtime_error("Failed to create progress bar");
        }
        m_progressBar->setVisible(false);
        m_progressBar->setTextVisible(false);
        m_progressBar->setFixedHeight(12);
        statusBar()->addPermanentWidget(m_progressBar, 0);

        // ===== CENTRAL WIDGET: Main content area with tabs =====
        m_contentTabs = new QTabWidget(this);
        if (!m_contentTabs) {
            throw std::runtime_error("Failed to create content tabs");
        }
            // Ensure we always have a central widget before returning
            setCentralWidget(m_contentTabs);

        // Events view tab
        m_eventsView = new EventsTableView(events, m_contentTabs);
        if (!m_eventsView) {
            throw std::runtime_error("Failed to create events view");
        }
        m_contentTabs->addTab(m_eventsView, "Events");

        // AI UI provided by plugins; initialize service holder only
        m_pluginService = nullptr;

        setCentralWidget(m_contentTabs);

    // ===== LEFT DOCK: Filters and AI Configuration =====
    // Filters dock
    m_filtersDock = new QDockWidget("Filters", this);
    if (!m_filtersDock) {
        throw std::runtime_error("Failed to create filters dock");
    }
    m_filtersDock->setObjectName("FiltersDockWidget");
    m_filtersDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_filtersDock->setFeatures(QDockWidget::DockWidgetMovable |
                               QDockWidget::DockWidgetFloatable |
                               QDockWidget::DockWidgetClosable);

    m_filterTabs = new QTabWidget(m_filtersDock);
    if (!m_filterTabs) {
        throw std::runtime_error("Failed to create filter tabs");
    }

    auto* filtersTab = new QWidget(m_filterTabs);
    if (!filtersTab) {
        throw std::runtime_error("Failed to create filters tab");
    }
    auto* filtersLayout = new QVBoxLayout(filtersTab);
    m_filtersPanel = new FiltersPanel(filtersTab);
    if (!m_filtersPanel) {
        throw std::runtime_error("Failed to create filters panel");
    }
    filtersLayout->addWidget(m_filtersPanel);
    filtersTab->setLayout(filtersLayout);

    auto* typeTab = new QWidget(m_filterTabs);
    if (!typeTab) {
        throw std::runtime_error("Failed to create type tab");
    }
    auto* typeLayout = new QVBoxLayout(typeTab);
    typeLayout->addWidget(new QLabel("Type:", typeTab));
    m_typeFilterView = new TypeFilterView(typeTab);
    if (!m_typeFilterView) {
        throw std::runtime_error("Failed to create type filter view");
    }
    typeLayout->addWidget(m_typeFilterView);
    m_applyFilterButton = new QPushButton("Apply Filter", typeTab);
    if (!m_applyFilterButton) {
        throw std::runtime_error("Failed to create apply filter button");
    }
    typeLayout->addWidget(m_applyFilterButton);
    typeTab->setLayout(typeLayout);

    m_filterTabs->addTab(filtersTab, "Extended Filters");
    m_filterTabs->addTab(typeTab, "Type Filters");

    // ── Actor Definitions tab (left panel) ───────────────────────────────
    m_actorDefPanel = new ActorDefinitionsPanel(m_filterTabs);
    m_filterTabs->addTab(m_actorDefPanel, "Actor Definitions");

    m_filtersDock->setWidget(m_filterTabs);
    addDockWidget(Qt::LeftDockWidgetArea, m_filtersDock);

    // Generic Plugin Configuration dock with tabs for multiple plugins
    m_pluginLeftDock = new QDockWidget("Plugin Configuration", this);
    if (!m_pluginLeftDock) {
        throw std::runtime_error("Failed to create plugin left/config dock");
    }
    m_pluginLeftDock->setObjectName("PluginConfigurationDockWidget");
    m_pluginLeftDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_pluginLeftDock->setFeatures(QDockWidget::DockWidgetMovable |
                                    QDockWidget::DockWidgetFloatable |
                                    QDockWidget::DockWidgetClosable);
    
    // Create tab widget to hold multiple plugin configurations
    m_pluginLeftTabs = new QTabWidget(m_pluginLeftDock);
    m_pluginLeftTabs->setObjectName("PluginConfigTabs");
    m_pluginLeftTabs->setTabsClosable(false);

    // Set size policy to allow the tab widget to shrink/expand with available space
    m_pluginLeftTabs->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);

    m_pluginLeftDock->setWidget(m_pluginLeftTabs);

    addDockWidget(Qt::LeftDockWidgetArea, m_pluginLeftDock);
    m_pluginLeftDock->setFloating(false);  // Ensure it's docked, not floating
    tabifyDockWidget(m_filtersDock, m_pluginLeftDock);  // Tab with filters in left panel
    m_pluginLeftDock->hide(); // Hidden until plugins provide configuration UI

    // ===== RIGHT DOCK: Item Details =====
    m_detailsDock = new QDockWidget("Item Details", this);
    if (!m_detailsDock) {
        throw std::runtime_error("Failed to create details dock");
    }
    m_detailsDock->setObjectName("ItemDetailsDockWidget");
    m_detailsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_detailsDock->setFeatures(QDockWidget::DockWidgetMovable |
                               QDockWidget::DockWidgetFloatable |
                               QDockWidget::DockWidgetClosable);

    m_itemDetailsView = new ItemDetailsView(events, m_detailsDock);
    if (!m_itemDetailsView) {
        throw std::runtime_error("Failed to create item details view");
    }
    m_detailsDock->setWidget(m_itemDetailsView);
    addDockWidget(Qt::RightDockWidgetArea, m_detailsDock);

    // ===== MAIN TAB: Statistics Summary =====
    m_statsPanel = new StatsSummaryPanel(events, m_eventsView, this);
    m_contentTabs->addTab(m_statsPanel, tr("Statistics"));

    // ===== MAIN TAB: Actors =====
    m_actorsPanel = new ActorsPanel(events, m_eventsView, this);
    m_contentTabs->addTab(m_actorsPanel, tr("Actors"));

    // ===== UPDATE CHECKER =====
    m_updateChecker = new UpdateChecker(this);
    connect(m_updateChecker, &UpdateChecker::UpdateCheckComplete,
            this,            &MainWindow::OnUpdateCheckComplete);

    // Status bar badge — hidden until an update is found
    m_updateBadge = new QLabel(tr("  ⬆ Update available  "), this);
    m_updateBadge->setStyleSheet(
        "color: white; background-color: #007AFF; border-radius: 3px;"
        " padding: 1px 4px; font-weight: bold;");
    m_updateBadge->setCursor(Qt::PointingHandCursor);
    m_updateBadge->setToolTip(tr("Click to view available updates"));
    m_updateBadge->hide();
    statusBar()->addPermanentWidget(m_updateBadge);
    // Make badge clickable via a transparent overlay QPushButton
    {
        auto* badgeBtn = new QPushButton(m_updateBadge);
        badgeBtn->setFlat(true);
        badgeBtn->setStyleSheet("background: transparent; border: none;");
        badgeBtn->resize(m_updateBadge->sizeHint());
        connect(badgeBtn, &QPushButton::clicked, this, &MainWindow::OnCheckForUpdates);
        badgeBtn->show();
    }

    // ===== BOTTOM DOCK: Search & AI Chat =====
    m_bottomDock = new QDockWidget("Tools", this);
    if (!m_bottomDock) {
        throw std::runtime_error("Failed to create bottom dock");
    }
    m_bottomDock->setObjectName("ToolsDockWidget");
    m_bottomDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    m_bottomDock->setFeatures(QDockWidget::DockWidgetMovable |
                              QDockWidget::DockWidgetFloatable |
                              QDockWidget::DockWidgetClosable);

    m_bottomTabs = new QTabWidget(m_bottomDock);
    if (!m_bottomTabs) {
        throw std::runtime_error("Failed to create bottom tabs");
    }

    // Search tab
    auto* searchPanel = new QWidget(m_bottomTabs);
    if (!searchPanel) {
        throw std::runtime_error("Failed to create search panel");
    }
    auto* searchLayout = new QVBoxLayout(searchPanel);
    searchLayout->setContentsMargins(4, 4, 4, 4);
    searchLayout->setSpacing(8);

    auto* searchRow = new QHBoxLayout();
    m_searchEdit = new QLineEdit(searchPanel);
    if (!m_searchEdit) {
        throw std::runtime_error("Failed to create search edit");
    }
    m_searchEdit->setPlaceholderText("Enter search query");
    m_searchButton = new QPushButton("Search", searchPanel);
    if (!m_searchButton) {
        throw std::runtime_error("Failed to create search button");
    }
    searchRow->addWidget(m_searchEdit);
    searchRow->addWidget(m_searchButton);

    m_searchResults = new SearchResultsView(searchPanel);
    if (!m_searchResults) {
        throw std::runtime_error("Failed to create search results view");
    }

    searchLayout->addLayout(searchRow);
    searchLayout->addWidget(m_searchResults, 1);
    searchPanel->setLayout(searchLayout);

    m_bottomTabs->addTab(searchPanel, "Search");

    // AI chat tab will be provided by AI plugin when active

    m_bottomDock->setWidget(m_bottomTabs);
    addDockWidget(Qt::BottomDockWidgetArea, m_bottomDock);

    // ===== WINDOW SETTINGS =====
    setAcceptDrops(true);
    const auto title = QStringLiteral("LogViewer Qt %1")
                           .arg(QString::fromStdString(Version::current().asShortStr()));
    setWindowTitle(title);
    setMinimumSize(1024, 768);

    connect(m_searchButton, &QPushButton::clicked, this,
        &MainWindow::OnSearchRequested);
    connect(m_searchEdit, &QLineEdit::returnPressed, this,
        &MainWindow::OnSearchRequested);
    connect(m_applyFilterButton, &QPushButton::clicked, this,
        &MainWindow::OnApplyFilterClicked);

    if (m_typeFilterView)
    {
        m_typeFilterView->SetOnFilterChanged(
            [this]() { HandleTypeFilterChanged(); });
    }

    if (m_filtersPanel)
    {
        connect(m_filtersPanel, &FiltersPanel::RequestApplyFilters, this,
            &MainWindow::OnExtendedFiltersChanged);
    }

    if (m_eventsView && m_itemDetailsView)
    {
        connect(m_eventsView, &EventsTableView::CurrentActualRowChanged,
            m_itemDetailsView, &ItemDetailsView::OnActualRowChanged);
    }

    util::Logger::Debug("[MainWindow] UI initialized");

    } catch (const std::exception& ex) {
        util::Logger::Error("[MainWindow] UI initialization failed: {}", ex.what());
        throw;
    }
}

void MainWindow::SetupMenus()
{
    auto* bar = menuBar();
    if (!bar)
    {
        bar = new QMenuBar(this);
        setMenuBar(bar);
    }
    bar->clear();

    auto* fileMenu = bar->addMenu(tr("&File"));
    auto* openAction = fileMenu->addAction(tr("&Open..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, [this]() {
        util::Logger::Debug("[MainWindow] Open menu triggered");
        OnOpenFileRequested();
    });

    // Add Recent Files submenu
    m_recentFilesMenu = fileMenu->addMenu(tr("Recent &Files"));
    m_recentFilesMenu->setEnabled(!m_recentFiles.empty());
    RefreshRecentFilesMenu();

    fileMenu->addSeparator();

    auto* exportMenu = fileMenu->addMenu(tr("E&xport"));
    auto* exportCsvAction  = exportMenu->addAction(tr("Export to &CSV..."));
    auto* exportJsonAction = exportMenu->addAction(tr("Export to &JSON..."));
    auto* exportXmlAction  = exportMenu->addAction(tr("Export to &XML..."));
    connect(exportCsvAction,  &QAction::triggered, this, &MainWindow::OnExportCsvRequested);
    connect(exportJsonAction, &QAction::triggered, this, &MainWindow::OnExportJsonRequested);
    connect(exportXmlAction,  &QAction::triggered, this, &MainWindow::OnExportXmlRequested);

    fileMenu->addSeparator();

    auto* clearAction = fileMenu->addAction(tr("&Clear Data"));
    clearAction->setShortcut(QKeySequence(tr("Ctrl+Shift+L")));
    connect(clearAction, &QAction::triggered, this,
        &MainWindow::OnClearDataRequested);

    fileMenu->addSeparator();

    auto* exitAction = fileMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this,
        &MainWindow::OnExitRequested);

    auto* toolsMenu = bar->addMenu(tr("&Tools"));

    auto* structuredConfigAction =
        toolsMenu->addAction(tr("Edit &Config..."));
    connect(structuredConfigAction, &QAction::triggered, this, [this]() {
        ui::qt::StructuredConfigDialog dlg(this);
        dlg.AddObserver(this);
        dlg.exec();
    });

    auto* editRawConfigAction =
        toolsMenu->addAction(tr("Edit Raw Config JSON..."));
    connect(editRawConfigAction, &QAction::triggered, this, [this]() {
        ui::qt::ConfigEditorDialog dlg(this);
        dlg.exec();
    });

    auto* appLogAction = toolsMenu->addAction(tr("Open &App Log"));
    connect(appLogAction, &QAction::triggered, this,
        &MainWindow::OnOpenAppLogRequested);

    toolsMenu->addSeparator();

    auto* reloadPluginsAction = toolsMenu->addAction(tr("&Reload Plugins"));
    reloadPluginsAction->setShortcut(QKeySequence(tr("Ctrl+Shift+P")));
    connect(reloadPluginsAction, &QAction::triggered, this, [this]() {
        reloadPlugins();
        QMessageBox::information(this, tr("Plugins"), tr("Plugins reloaded successfully"));
    });

    // View menu for dock widgets
    auto* viewMenu = bar->addMenu(tr("&View"));
    viewMenu->addAction(m_filtersDock->toggleViewAction());
    viewMenu->addAction(m_pluginLeftDock->toggleViewAction());
    viewMenu->addAction(m_detailsDock->toggleViewAction());
    viewMenu->addAction(m_bottomDock->toggleViewAction());
    
    viewMenu->addSeparator();
    
    // Theme submenu
    auto* themeMenu = viewMenu->addMenu(tr("&Theme"));
    
    auto* darkThemeAction = themeMenu->addAction(tr("&Dark"));
    connect(darkThemeAction, &QAction::triggered, this, &MainWindow::OnSetDarkTheme);
    
    auto* lightThemeAction = themeMenu->addAction(tr("&Light"));
    connect(lightThemeAction, &QAction::triggered, this, &MainWindow::OnSetLightTheme);
    
    auto* systemThemeAction = themeMenu->addAction(tr("&System"));
    connect(systemThemeAction, &QAction::triggered, this, &MainWindow::OnSetSystemTheme);
    
    viewMenu->addSeparator();
    
    auto* resetLayoutAction = viewMenu->addAction(tr("&Reset Layout"));
    connect(resetLayoutAction, &QAction::triggered, this, [this]() {
        // Reset all docks to default positions
        if (m_filtersDock) {
            m_filtersDock->setFloating(false);
            addDockWidget(Qt::LeftDockWidgetArea, m_filtersDock);
            m_filtersDock->show();
        }
        if (m_pluginLeftDock) {
            m_pluginLeftDock->setFloating(false);
            addDockWidget(Qt::LeftDockWidgetArea, m_pluginLeftDock);
            // Only show if there are config tabs
            if (m_pluginLeftTabs && m_pluginLeftTabs->count() > 0) {
                m_pluginLeftDock->show();
            }
        }
        if (m_detailsDock) {
            m_detailsDock->setFloating(false);
            addDockWidget(Qt::RightDockWidgetArea, m_detailsDock);
            m_detailsDock->show();
        }
        if (m_bottomDock) {
            m_bottomDock->setFloating(false);
            addDockWidget(Qt::BottomDockWidgetArea, m_bottomDock);
            m_bottomDock->show();
        }
    });

    // Help menu
    auto* helpMenu = bar->addMenu(tr("&Help"));
    auto* checkUpdatesAction = helpMenu->addAction(tr("Check for &Updates..."));
    connect(checkUpdatesAction, &QAction::triggered, this,
            &MainWindow::OnCheckForUpdates);
    helpMenu->addSeparator();
    auto* aboutAction = helpMenu->addAction(tr("&About"));
    connect(aboutAction, &QAction::triggered, this, [this]() {
        OnAboutRequested();
    });
}

void MainWindow::InitializePresenter(mvc::IController& controller,
    db::EventsContainer& events)
{
    util::Logger::Debug("[MainWindow] InitializePresenter");

    // Validate that all required UI components are properly initialized
    if (!m_eventsView) {
        throw std::runtime_error("Events view not initialized");
    }
    if (!m_searchResults) {
        throw std::runtime_error("Search results view not initialized");
    }
    if (!m_typeFilterView) {
        throw std::runtime_error("Type filter view not initialized");
    }
    if (!m_itemDetailsView) {
        throw std::runtime_error("Item details view not initialized");
    }

    // Create the presenter with all required interfaces
    m_presenter = std::make_unique<ui::MainWindowPresenter>(
        *this,                    // IMainWindowView
        controller,               // IController
        events,                   // EventsContainer
        *m_searchResults,         // ISearchResultsView
        m_eventsView,             // IEventsListView
        m_typeFilterView,         // ITypeFilterView
        m_itemDetailsView         // IItemDetailsView
    );

    // Set up observers
    m_searchResults->SetObserver(this);

    // Connect stats panel to model resets (covers both data load and filter changes)
    if (m_statsPanel && m_eventsView && m_eventsView->model()) {
        connect(m_eventsView->model(), &QAbstractItemModel::modelReset,
                m_statsPanel, &StatsSummaryPanel::Refresh);
    }

    // Connect actors panel to model resets
    if (m_actorsPanel && m_eventsView && m_eventsView->model()) {
        connect(m_eventsView->model(), &QAbstractItemModel::modelReset,
                m_actorsPanel, &ActorsPanel::Refresh);
    }

    // Connect actor definitions → actors panel
    if (m_actorDefPanel && m_actorsPanel) {
        connect(m_actorDefPanel, &ActorDefinitionsPanel::DefinitionsChanged,
                m_actorsPanel,   &ActorsPanel::SetDefinitions);
        // Push any already-loaded definitions immediately
        m_actorsPanel->SetDefinitions(m_actorDefPanel->Definitions());
    }

    // Schedule automatic update check (delayed so the UI is fully shown first)
    if (m_updateChecker && ShouldCheckForUpdates())
    {
        QTimer::singleShot(3000, m_updateChecker, &UpdateChecker::CheckAsync);
        util::Logger::Info("[MainWindow] Update check scheduled (startup)");
    }

    util::Logger::Debug("[MainWindow] Presenter initialized successfully");
}

void MainWindow::LoadRecentFiles()
{
    QSettings settings("LogViewer", "LogViewer");
    m_recentFiles.clear();
    
    int size = settings.beginReadArray("RecentFiles");
    for (int i = 0; i < size; ++i) {
        settings.setArrayIndex(i);
        QString path = settings.value("path", "").toString();
        if (!path.isEmpty() && std::filesystem::exists(path.toStdString())) {
            m_recentFiles.push_back(path);
        }
    }
    settings.endArray();
    
    util::Logger::Debug("[MainWindow] Loaded {} recent files", m_recentFiles.size());
}

void MainWindow::SaveRecentFiles()
{
    QSettings settings("LogViewer", "LogViewer");
    settings.beginWriteArray("RecentFiles");
    
    for (int i = 0; i < static_cast<int>(m_recentFiles.size()); ++i) {
        settings.setArrayIndex(i);
        settings.setValue("path", m_recentFiles[static_cast<std::size_t>(i)]);
    }
    
    settings.endArray();
    util::Logger::Debug("[MainWindow] Saved {} recent files", m_recentFiles.size());
}

void MainWindow::AddToRecentFiles(const QString& filePath)
{
    if (filePath.isEmpty()) return;
    
    // Remove if already in list
    auto it = std::find(m_recentFiles.begin(), m_recentFiles.end(), filePath);
    if (it != m_recentFiles.end()) {
        m_recentFiles.erase(it);
    }
    
    // Add to front
    m_recentFiles.insert(m_recentFiles.begin(), filePath);
    
    // Keep only MAX_RECENT_FILES
    if (m_recentFiles.size() > MAX_RECENT_FILES) {
        m_recentFiles.resize(MAX_RECENT_FILES);
    }
    
    // Refresh menu
    if (m_recentFilesMenu) {
        m_recentFilesMenu->setEnabled(!m_recentFiles.empty());
        RefreshRecentFilesMenu();
    }
    
    // Save to settings
    SaveRecentFiles();
    
    util::Logger::Debug("[MainWindow] Added to recent files: {}", filePath.toStdString());
}

void MainWindow::RefreshRecentFilesMenu()
{
    if (!m_recentFilesMenu) return;
    
    m_recentFilesMenu->clear();
    
    if (m_recentFiles.empty()) {
        auto* emptyAction = m_recentFilesMenu->addAction(tr("(Empty)"));
        emptyAction->setEnabled(false);
        return;
    }
    
    for (const auto& file : m_recentFiles) {
        QFileInfo fileInfo(file);
        QString displayName = fileInfo.fileName();
        QString fullPath = file;
        
        auto* action = m_recentFilesMenu->addAction(displayName);
        action->setToolTip(fullPath);
        
        connect(action, &QAction::triggered, this, [this, fullPath]() {
            OnRecentFileTriggered(fullPath);
        });
    }
    
    m_recentFilesMenu->addSeparator();
    auto* clearAction = m_recentFilesMenu->addAction(tr("Clear Recent Files"));
    connect(clearAction, &QAction::triggered, this, [this]() {
        m_recentFiles.clear();
        SaveRecentFiles();
        if (m_recentFilesMenu) {
            m_recentFilesMenu->setEnabled(false);
            RefreshRecentFilesMenu();
        }
    });
}

void MainWindow::OnRecentFileTriggered(const QString& filePath)
{
    util::Logger::Debug("[MainWindow] Recent file triggered: {}", filePath.toStdString());
    HandleDroppedFile(filePath);
}

std::string MainWindow::ReadSearchQuery() const
{
    return m_searchEdit->text().toStdString();
}

std::string MainWindow::CurrentStatusText() const
{
    return m_statusLabel->text().toStdString();
}

void MainWindow::UpdateStatusText(const std::string& text)
{
    m_statusLabel->setText(QString::fromStdString(text));
}

void MainWindow::SetSearchControlsEnabled(bool enabled)
{
    m_searchEdit->setEnabled(enabled);
    m_searchButton->setEnabled(enabled);
}

void MainWindow::ToggleProgressVisibility(bool visible)
{
    m_progressBar->setVisible(visible);
}

void MainWindow::ConfigureProgressRange(int range)
{
    m_progressBar->setRange(0, range);
}

void MainWindow::UpdateProgressValue(int value)
{
    m_progressBar->setValue(value);
}

void MainWindow::ProcessPendingEvents()
{
    QCoreApplication::processEvents();
}

void MainWindow::RefreshLayout()
{
    if (auto* widget = centralWidget())
        widget->updateGeometry();
    this->update();
}

void MainWindow::OnSearchResultActivated(long eventId)
{
        util::Logger::Debug("[MainWindow] OnSearchResultActivated eventId={}",
            eventId);
        for (long i = 0u; i < static_cast<long>(m_events->Size()); ++i)
        {
            if (static_cast<long>(m_events->GetEvent(static_cast<int>(i)).getId()) == eventId)            {
                util::Logger::Debug(
                    "[MainWindow] Matching event found at index={}", static_cast<long long>(i));
                m_events->SetCurrentItem(static_cast<int>(i));
                break;
            }
        }
}

void MainWindow::OnSearchRequested()
{
    util::Logger::Debug("[MainWindow] OnSearchRequested query='{}'",
        ReadSearchQuery());
    if (m_presenter)
        m_presenter->PerformSearch();
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    if (event && event->mimeData() && event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else if (event)
        event->ignore();
}

void MainWindow::dropEvent(QDropEvent* event)
{
    if (!event || !event->mimeData() || !event->mimeData()->hasUrls())
        return;

    const auto urls = event->mimeData()->urls();
    if (urls.isEmpty())
        return;

    for (const auto& url : urls)
    {
        if (url.isLocalFile())
        {
            util::Logger::Info("[MainWindow] Dropped file: {}",
                url.toLocalFile().toStdString());
            HandleDroppedFile(url.toLocalFile());
            break; // align with wx: only first file processed for now
        }
    }

    event->acceptProposedAction();
}

void MainWindow::HandleDroppedFile(const QString& path)
{
    if (path.isEmpty())
    {
        QMessageBox::warning(this, "File Drop", "Dropped file path is empty.");
        return;
    }

    const std::filesystem::path filePath(path.toStdString());

    util::Logger::Info("[MainWindow] HandleDroppedFile path={}",
        filePath.string());

    if (!m_presenter)
    {
        QMessageBox::warning(this, "File Drop",
            "Presenter is not ready to load files.");
        return;
    }

    try
    {
        // Check if there's existing data
        if (m_events->Size() > 0)
        {
            // Show dialog to ask user what to do
            LogFileLoadDialog dialog(QString::fromStdString(filePath.filename().string()), this);
            
#ifdef __APPLE__
            // Workaround for macOS native dialog issues
            dialog.setWindowModality(Qt::WindowModal);
#endif
            
            if (dialog.exec() == QDialog::Accepted)
            {
                if (dialog.GetLoadMode() == LogFileLoadDialog::LoadMode::Replace)
                {
                    // Replace existing data
                    const QString message = QString("Loading %1 ...").arg(path);
                    UpdateStatusText(message.toStdString());
                    m_presenter->LoadLogFile(filePath);
                    m_presenter->SetItemDetailsVisible(true);
                    const QString readyMsg = QString("Data ready. Path: %1").arg(path);
                    UpdateStatusText(readyMsg.toStdString());
                    AddToRecentFiles(path);  // Add to recent files
                }
                else // Merge
                {
                    // Merge with existing data
                    const std::string existingAlias = dialog.GetExistingFileAlias().toStdString();
                    const std::string newFileAlias = dialog.GetNewFileAlias().toStdString();
                    const QString mergingMsg = QString("Merging %1 ...").arg(path);
                    UpdateStatusText(mergingMsg.toStdString());
                    m_presenter->MergeLogFile(filePath, existingAlias, newFileAlias);
                    m_presenter->SetItemDetailsVisible(true);
                    const QString completeMsg = QString("Merge complete. Path: %1").arg(path);
                    UpdateStatusText(completeMsg.toStdString());
                    AddToRecentFiles(path);  // Add to recent files
                }
            }
            // If dialog was canceled, do nothing
        }
        else
        {
            // No existing data, just load normally
            const QString message = QString("Loading %1 ...").arg(path);
            UpdateStatusText(message.toStdString());
            m_presenter->LoadLogFile(filePath);
            m_presenter->SetItemDetailsVisible(true);
            const QString readyMsg = QString("Data ready. Path: %1").arg(path);
            UpdateStatusText(readyMsg.toStdString());
            AddToRecentFiles(path);  // Add to recent files
        }
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("[MainWindow] Failed to load file '{}': {}",
            filePath.string(), ex.what());
        const QString failedMsg = QString("Failed to load  complete file Path: %1").arg(path);
        UpdateStatusText(failedMsg.toStdString());
        const QString errorMsg = QString("Unable to load %1\n%2").arg(path).arg(ex.what());
        QMessageBox::critical(this, "File Drop Error", errorMsg);
    }
}

void MainWindow::OnApplyFilterClicked()
{
    if (m_presenter)
        m_presenter->ApplySelectedTypeFilters();
}

void MainWindow::HandleTypeFilterChanged()
{
    if (m_presenter)
        m_presenter->ApplySelectedTypeFilters();
}

void MainWindow::OnExtendedFiltersChanged()
{
    ApplyExtendedFilters();
}

void MainWindow::OnOpenFileRequested()
{
    util::Logger::Debug("[MainWindow] OnOpenFileRequested: opening QFileDialog");

    QFileDialog dialog(this, tr("Open Log File"));
    #ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
    #endif
    dialog.setNameFilter(tr("Log files (*.log *.txt *.xml *.csv);;All files (*.*)"));
    if (dialog.exec() != QDialog::Accepted) {
        util::Logger::Debug("[MainWindow] OnOpenFileRequested: dialog cancelled");
        return;
    }
    const QString filePath = dialog.selectedFiles().value(0);

    if (filePath.isEmpty())
    {
        util::Logger::Debug("[MainWindow] OnOpenFileRequested: No file selected");
        return;
    }

    util::Logger::Info("[MainWindow] OnOpenFileRequested path={}",
        filePath.toStdString());

    HandleDroppedFile(filePath);
}

void MainWindow::OnClearDataRequested()
{
    try
    {
    util::Logger::Info("[MainWindow] OnClearDataRequested");
        if (m_searchResults)
            m_searchResults->Clear();
        if (m_searchEdit)
            m_searchEdit->clear();
        if (m_events)
            m_events->Clear();
        if (m_eventsView)
        {
            m_eventsView->RefreshView();
            m_eventsView->RefreshColumns();  // Hide Source and original_id columns after clearing
        }
        if (m_itemDetailsView)
            m_itemDetailsView->RefreshView();
        UpdateStatusText("Data cleared");
        ToggleProgressVisibility(false);
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("[MainWindow] Clear data failed: {}", ex.what());
        ShowError(tr("Clear Data"), tr("Unable to clear data: %1").arg(ex.what()));
    }
}

void MainWindow::OnOpenAppLogRequested()
{
    try
    {
        const auto& logPath = config::GetConfig().GetAppLogPath();
        if (logPath.empty() || !std::filesystem::exists(logPath))
        {
            util::Logger::Warn(
                "[MainWindow] Application log file does not exist: '{}'",
                logPath);
            ShowError(tr("App Log"), tr("Application log file does not exist."));
            return;
        }

        if (!QDesktopServices::openUrl(
                QUrl::fromLocalFile(QString::fromStdString(logPath))))
        {
            util::Logger::Error(
                "[MainWindow] Failed to open application log: '{}'",
                logPath);
            ShowError(tr("App Log"), tr("Failed to open application log."));
        }
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("[MainWindow] Unable to open application log: {}",
            ex.what());
        ShowError(tr("App Log"), tr("Unable to open application log: %1").arg(ex.what()));
    }
}

void MainWindow::OnExitRequested()
{
    close();
}

void MainWindow::ApplyExtendedFilters()
{
    if (!m_eventsView || !m_events || m_events->Size() == 0)
        return;

    const std::string previousStatus = CurrentStatusText();
    UpdateStatusText("Applying filters...");

    auto filteredIndices =
        filters::FilterManager::getInstance().applyFilters(*m_events);

    util::Logger::Debug("[MainWindow] ApplyExtendedFilters: {} events, {} matches",
        m_events->Size(), filteredIndices.size());

    m_eventsView->SetFilteredEvents(filteredIndices);
    m_eventsView->RefreshView();

    UpdateStatusText(previousStatus);
}

void MainWindow::ShowError(const QString& title, const QString& message)
{
    QMessageBox::critical(this, title, message);
}

void MainWindow::OnConfigChanged()
{
    util::Logger::Debug("[MainWindow] OnConfigChanged");
    
    // Refresh views with new configuration
    if (m_eventsView)
    {
        m_eventsView->UpdateColors();
        m_eventsView->RefreshColumns();
        m_eventsView->RefreshView();
    }
    
    if (m_itemDetailsView)
        m_itemDetailsView->RefreshView();
    
    // Refresh AI provider (built-in or plugin)
    RefreshPluginPanels();
}

void MainWindow::OnAboutRequested()
{
    const auto& version = Version::current();
    
    QString aboutText = tr(
        "<h2>LogViewer</h2>"
        "<p>A modern, cross-platform log viewer with AI-assisted analysis, flexible filtering, and customizable visualization.</p>"
        "<p><b>Version:</b> %1</p>"
        "<p><b>Build Date:</b> %2</p>"
        "<p><b>Author:</b> LogViewer Development Team</p>"
        "<p><b>Copyright:</b> © 2022-2025 LogViewer Contributors</p>"
        "<p><b>Qt Version:</b> %3</p>"
        "<p><br/>Licensed under Proprietary License</p>"
    ).arg(QString::fromStdString(version.asShortStr()),
         QString::fromStdString(version.datetime),
         QString::fromStdString(QT_VERSION_STR));
    
    QMessageBox::about(this, tr("About LogViewer"), aboutText);
}

void MainWindow::setupPluginManager() {
    auto& pluginManager = plugin::PluginManager::GetInstance();
    
    // Register this window as an observer for plugin events
    pluginManager.RegisterObserver(this);
    
    // Set plugins directory relative to application executable
    std::filesystem::path pluginsDir;
    
    pluginsDir = config::GetConfig().GetDefaultAppPath() / "plugins";
    
    pluginManager.SetPluginsDirectory(pluginsDir);
    util::Logger::Info("[MainWindow] Plugin directory set to: {}", pluginsDir.string());

    // Register EventsContainer callbacks so plugins can access events via C ABI
    if (m_events) {
        PluginEvents_Register(reinterpret_cast<void*>(m_events),
                              &PluginEvents_GetSizeBridge,
                              &PluginEvents_GetEventJsonBridge);
        util::Logger::Debug("[MainWindow] Registered EventsContainer bridge for plugins");
        // Also inform PluginManager about the opaque events pointer so it can pass
        // the same opaque handle to AI provider plugins via C API.
        plugin::PluginManager::GetInstance().SetEventsContainerOpaque(reinterpret_cast<void*>(m_events));
        // Also provide the callback function pointers so PluginManager can pass
        // them into plugins (required for cross-DLL event access).
        plugin::PluginManager::GetInstance().SetEventsCallbacks(&PluginEvents_GetSizeBridge,
                                                                &PluginEvents_GetEventJsonBridge);

        PluginHostUiCallbacks hostUi {};
        hostUi.size = static_cast<uint32_t>(sizeof(PluginHostUiCallbacks));
        hostUi.setCurrentItem = &PluginHostUi_SetCurrentItemBridge;
        plugin::PluginManager::GetInstance().SetHostUiCallbacks(reinterpret_cast<void*>(m_events), hostUi);
    } else {
        util::Logger::Warn("[MainWindow] No EventsContainer available to register with plugins");
    }
}

void MainWindow::loadPlugins() {
    auto& pluginManager = plugin::PluginManager::GetInstance();
    
    // Load plugin configuration (states, auto-load settings)
    auto configResult = pluginManager.LoadConfiguration();
    if (configResult.isErr()) {
        util::Logger::Warn("[MainWindow] Failed to load plugin configuration: {}", 
            configResult.error().what());
    }
    
    // Discover plugins from the plugins directory
    auto discoveredPlugins = pluginManager.DiscoverPlugins();
    util::Logger::Info("[MainWindow] Discovered {} plugins", discoveredPlugins.size());
    
    for (const auto& pluginPath : discoveredPlugins) {
        // Load the plugin
        auto loadResult = pluginManager.LoadPlugin(pluginPath);
        if (loadResult.isErr()) {
            util::Logger::Error("[MainWindow] Failed to load plugin: {}",
                pluginPath.string());
            continue;
        }
        
        std::string pluginId = loadResult.unwrap();
        
        // Check if plugin should be auto-enabled based on autoLoad setting
        const auto& loadedPlugins = pluginManager.GetLoadedPlugins();
        auto it = loadedPlugins.find(pluginId);
        if (it != loadedPlugins.end() && it->second.autoLoad) {
            util::Logger::Info("[MainWindow] Enabling plugin: {} (autoLoad=true)", pluginId);
            auto enableResult = pluginManager.EnablePlugin(pluginId);
            if (enableResult.isErr()) {
                util::Logger::Error("[MainWindow] Failed to enable plugin {}: {}",
                    pluginId, enableResult.error().what());
            }
        }
    }

    RefreshPluginPanels();
}

// Inline removal implementations into generic wrappers and remove AI-specific symbols

// Generic wrappers keeping compatibility while removing AI-specific naming
void MainWindow::RemoveMainPanel()
{
    if (!m_contentTabs || !m_mainPanelWidget)
        return;

    const int idx = m_contentTabs->indexOf(m_mainPanelWidget);
    if (idx >= 0)
    {
        m_contentTabs->removeTab(idx);
    }
    m_mainPanelWidget->deleteLater();
    m_mainPanelWidget = nullptr;
    m_mainPanelIndex = -1;
}

void MainWindow::RemoveLeftPanel()
{
    // Remove all plugin-provided left/filter and config tabs
    std::vector<std::string> ids;
    ids.reserve(m_pluginFilterTabIndices.size() + m_pluginLeftTabIndices.size());
    for (const auto& [id, idx] : m_pluginFilterTabIndices) ids.push_back(id);
    for (const auto& [id, idx] : m_pluginLeftTabIndices) ids.push_back(id);
    for (const auto& id : ids) removePluginLeftTab(id);
}

void MainWindow::RemoveBottomPanel()
{
    if (!m_bottomTabs || !m_bottomChatWidget)
        return;

    const int idx = m_bottomTabs->indexOf(m_bottomChatWidget);
    if (idx >= 0)
    {
        m_bottomTabs->removeTab(idx);
    }
    m_bottomChatWidget->deleteLater();
    m_bottomChatWidget = nullptr;
}

void MainWindow::RemoveRightPanel()
{
    // Remove all plugin-provided right dock tabs
    std::vector<std::string> ids;
    ids.reserve(m_pluginRightTabIndices.size());
    for (const auto& [id, idx] : m_pluginRightTabIndices) ids.push_back(id);
    for (const auto& id : ids) {
        auto it = m_pluginRightTabIndices.find(id);
        if (it == m_pluginRightTabIndices.end()) continue;
        int tabIndex = it->second;
        if (m_rightTabs && tabIndex >= 0 && tabIndex < m_rightTabs->count()) {
            QWidget* widget = m_rightTabs->widget(tabIndex);
            m_rightTabs->removeTab(tabIndex);
            if (widget) widget->deleteLater();
            util::Logger::Info("[MainWindow] Removed plugin right-panel at index {}", tabIndex);
        }
        m_pluginRightTabIndices.erase(it);
        for (auto& [pid, pidx] : m_pluginRightTabIndices) if (pidx > tabIndex) pidx--;
    }
    m_activePluginId.clear();
}

void MainWindow::RefreshPluginPanels()
{
    // Plugins are responsible for creating/managing their own UI panels via the SDK/C-ABI.
    // This function is kept for potential future use but currently does nothing.
    util::Logger::Debug("[MainWindow] RefreshPluginPanels called (no-op)");
}

// Helper: create a small host-owned container and parent the plugin widget into it
QWidget* MainWindow::CreateHostContainerForPluginWidget(QWidget* pluginWidget, QTabWidget* parentTabs) {
    if (!pluginWidget || !parentTabs) return nullptr;
    QWidget* hostContainer = new QWidget(parentTabs);
    auto* layout = new QVBoxLayout(hostContainer);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    hostContainer->setLayout(layout);
    try { pluginWidget->setParent(hostContainer); } catch(...) {}
    pluginWidget->setVisible(true);
    layout->addWidget(pluginWidget);
    return hostContainer;
}

bool MainWindow::TryAddPluginMainPanel(const std::string& pluginId, plugin::IPlugin* plugin) {
    if (!m_contentTabs || !plugin) return false;
    auto& manager = plugin::PluginManager::GetInstance();
    const auto& loaded = manager.GetLoadedPlugins();
    auto it = loaded.find(pluginId);
    if (it == loaded.end()) return false;
    if (!(it->second.pluginCreateMainPanel && it->second.pluginOpaqueHandle)) return false;
    using CreatePanelFn = void*(*)(void*, void*, const char*);
    auto fn = reinterpret_cast<CreatePanelFn>(it->second.pluginCreateMainPanel);
    void* w = nullptr;
    try { w = fn(it->second.pluginOpaqueHandle, static_cast<void*>(m_contentTabs), nullptr); } catch(...) { w = nullptr; }
    util::Logger::Debug("[MainWindow] TryAddPluginMainPanel for {}: widget returned = {}", pluginId, w != nullptr);
    if (!w) return false;
    QWidget* pluginWidget = reinterpret_cast<QWidget*>(w);
    QWidget* hostContainer = CreateHostContainerForPluginWidget(pluginWidget, m_contentTabs);
    if (!hostContainer) return false;
    const std::string tabName = plugin->GetMetadata().name.empty() ? pluginId : plugin->GetMetadata().name;
    int idx = m_contentTabs->addTab(hostContainer, QString::fromStdString(tabName));
    m_pluginTabIndices[pluginId] = idx;
    util::Logger::Info("[MainWindow] Added plugin main tab '{}' at index {}", tabName, idx);
    return true;
}

bool MainWindow::TryAddPluginBottomPanel(const std::string& pluginId, plugin::IPlugin* plugin) {
    if (!m_bottomTabs || !plugin) return false;
    auto& manager = plugin::PluginManager::GetInstance();
    const auto& loaded = manager.GetLoadedPlugins();
    auto it = loaded.find(pluginId);
    if (it == loaded.end()) return false;
    if (!(it->second.pluginCreateBottomPanel && it->second.pluginOpaqueHandle)) return false;
    using CreatePanelFn = void*(*)(void*, void*, const char*);
    auto fn = reinterpret_cast<CreatePanelFn>(it->second.pluginCreateBottomPanel);
    void* w = nullptr;
    try { w = fn(it->second.pluginOpaqueHandle, static_cast<void*>(m_bottomTabs), nullptr); } catch(...) { w = nullptr; }
    util::Logger::Debug("[MainWindow] TryAddPluginBottomPanel for {}: widget returned = {}", pluginId, w != nullptr);
    if (!w) return false;
    QWidget* pluginWidget = reinterpret_cast<QWidget*>(w);
    QWidget* hostContainer = CreateHostContainerForPluginWidget(pluginWidget, m_bottomTabs);
    if (!hostContainer) return false;
    const std::string tabName = plugin->GetMetadata().name.empty() ? pluginId : plugin->GetMetadata().name + " Chat";
    int chatTabIndex = m_bottomTabs->addTab(hostContainer, QString::fromStdString(tabName));
    m_bottomPluginPanel = hostContainer;
    util::Logger::Info("[MainWindow] Added plugin bottom tab '{}' at index {}", tabName, chatTabIndex);
    return true;
}

bool MainWindow::TryAddPluginRightPanel(const std::string& pluginId, plugin::IPlugin* plugin) {
    if (!plugin) return false;
    auto& manager = plugin::PluginManager::GetInstance();
    const auto& loaded = manager.GetLoadedPlugins();
    auto it = loaded.find(pluginId);
    if (it == loaded.end()) return false;
    if (!(it->second.pluginCreateRightPanel && it->second.pluginOpaqueHandle)) return false;
    using CreatePanelFn = void*(*)(void*, void*, const char*);
    auto fn = reinterpret_cast<CreatePanelFn>(it->second.pluginCreateRightPanel);
    void* w = nullptr;
    try { w = fn(it->second.pluginOpaqueHandle, static_cast<void*>(m_detailsDock ? m_detailsDock : nullptr), nullptr); } catch(...) { w = nullptr; }
    if (!w) return false;

    // Ensure we have a tab widget in the right dock
    if (!m_rightTabs) {
        m_rightTabs = new QTabWidget(m_detailsDock);
        if (m_itemDetailsView) {
            m_rightTabs->addTab(m_itemDetailsView, "Details");
        }
        m_detailsDock->setWidget(m_rightTabs);
    }

    QWidget* pluginWidget = reinterpret_cast<QWidget*>(w);
    QWidget* hostContainer = CreateHostContainerForPluginWidget(pluginWidget, m_rightTabs);
    if (!hostContainer) return false;

    const std::string tabName = plugin->GetMetadata().name.empty() ? pluginId : plugin->GetMetadata().name + " Right";
    int idx = m_rightTabs->addTab(hostContainer, QString::fromStdString(tabName));
    m_pluginRightTabIndices[pluginId] = idx;
    if (m_detailsDock) {
        m_detailsDock->setFloating(false);
        addDockWidget(Qt::RightDockWidgetArea, m_detailsDock);
        m_detailsDock->show();
    }
    util::Logger::Info("[MainWindow] Added plugin right tab '{}' at index {}", tabName, idx);
    return true;
}

void MainWindow::OnPluginEvent(plugin::PluginEvent event, 
                              const std::string& pluginId,
                              plugin::IPlugin* plugin) {
    using plugin::PluginEvent;
    
    switch (event) {
        case PluginEvent::Loaded:
            util::Logger::Debug("[MainWindow] Plugin loaded: {}", pluginId);
            break;
            
        case PluginEvent::Unloaded:
            util::Logger::Debug("[MainWindow] Plugin unloaded: {}", pluginId);
            if (pluginId == m_activePluginId) {
                RemoveMainPanel();
                RemoveLeftPanel();
                RemoveBottomPanel();
                RemoveRightPanel();
            }
            break;

        case PluginEvent::Enabled:
            util::Logger::Info("[MainWindow] Plugin enabled: {}", pluginId);
                    createPluginTab(pluginId, plugin);
                    createPluginLeftTab(pluginId, plugin);
            // If the plugin exposes C-ABI panel creators, embed those widgets
            // into main/bottom panels regardless of declared plugin type.
            {
                auto& manager = plugin::PluginManager::GetInstance();
                const auto& loaded = manager.GetLoadedPlugins();
                auto it = loaded.find(pluginId);
                if (it != loaded.end()) {
                    util::Logger::Debug("[MainWindow] Panel creation check for {}: mainExp={} bottomExp={} opaque={} contentTabs={} bottomTabs={}",
                        pluginId,
                        it->second.pluginCreateMainPanel != nullptr,
                        it->second.pluginCreateBottomPanel != nullptr,
                        it->second.pluginOpaqueHandle != nullptr,
                        m_contentTabs != nullptr,
                        m_bottomTabs != nullptr);
                    
                    // Use helper methods for consistent widget wrapping
                    TryAddPluginMainPanel(pluginId, plugin);
                    TryAddPluginBottomPanel(pluginId, plugin);
                    TryAddPluginRightPanel(pluginId, plugin);
                }
            }
            break;
            
        case PluginEvent::Disabled:
            util::Logger::Info("[MainWindow] Plugin disabled: {}", pluginId);
            removePluginTab(pluginId);
            removePluginLeftTab(pluginId);
            if (pluginId == m_activePluginId) {
                RemoveMainPanel();
                RemoveLeftPanel();
                RemoveBottomPanel();
                RemoveRightPanel();
                util::Logger::Info("[MainWindow] Plugin disabled and panels removed: {}", pluginId);
            }
            break;
            
        case PluginEvent::Registered:
            util::Logger::Info("[MainWindow] Plugin registered: {}", pluginId);
            break;
            
        case PluginEvent::Unregistered:
            util::Logger::Info("[MainWindow] Plugin unregistered: {}", pluginId);
            break;
    }
}

void MainWindow::createPluginTab(const std::string& pluginId, plugin::IPlugin* plugin) {
    if (!plugin) {
        util::Logger::Warn("[MainWindow] createPluginTab called with null plugin");
        return;
    }
    
    if (!m_contentTabs) {
        util::Logger::Error("[MainWindow] createPluginTab called but m_contentTabs is null");
        return;
    }
    
    util::Logger::Info("[MainWindow] Creating tab for plugin: {}", pluginId);
    
    // If the plugin exposes SDK-first panel creators, the host will embed
    // widgets via the C-ABI exports; skip calling the legacy CreateTab in
    // that case to avoid duplicate UI.
    {
        auto& pluginManager = plugin::PluginManager::GetInstance();
        const auto& loaded = pluginManager.GetLoadedPlugins();
        auto it = loaded.find(pluginId);
        if (it != loaded.end() && (it->second.pluginCreateMainPanel || it->second.pluginCreateBottomPanel || it->second.pluginCreateLeftPanel || it->second.pluginCreateRightPanel)) {
            util::Logger::Info("[MainWindow] Skipping immediate tab creation for SDK-first plugin: {}", pluginId);
            return;
        }
    }

    QWidget* tab = plugin->CreateTab(this);
    if (!tab) {
        util::Logger::Info("[MainWindow] Plugin {} does not provide a tab widget", pluginId);
        return;
    }
    
    auto metadata = plugin->GetMetadata();
    QString tabName = QString::fromStdString(metadata.name);
    int tabIndex = m_contentTabs->addTab(tab, tabName);
    m_pluginTabIndices[pluginId] = tabIndex;
    util::Logger::Info("[MainWindow] Created plugin tab: {} at index {}", metadata.name, tabIndex);
    
    // Analysis plugins should access events via PluginEvents_* helpers.
}

void MainWindow::removePluginTab(const std::string& pluginId) {
    util::Logger::Info("[MainWindow] Removing tab for plugin: {}", pluginId);
    
    // Find and remove the tab for this plugin
    auto it = m_pluginTabIndices.find(pluginId);
    if (it != m_pluginTabIndices.end()) {
        int tabIndex = it->second;
        
        // Remove the tab
        if (tabIndex >= 0 && tabIndex < m_contentTabs->count()) {
            QWidget* widget = m_contentTabs->widget(tabIndex);
            m_contentTabs->removeTab(tabIndex);
            if (widget) {
                widget->deleteLater();
            }
            util::Logger::Info("[MainWindow] Removed plugin tab at index {}", tabIndex);
        }
        
        // Update indices for tabs that came after this one
        m_pluginTabIndices.erase(it);
        for (auto& [id, idx] : m_pluginTabIndices) {
            if (idx > tabIndex) {
                idx--;
            }
        }
    }
}

// Legacy filter-tab creation removed. Left/tab management is done by
// createPluginLeftTab/removePluginLeftTab which handles both left-dock
// insertion and plugin config dock fallback.

void MainWindow::createPluginLeftTab(const std::string& pluginId, plugin::IPlugin* plugin) {
    if (!plugin) {
        util::Logger::Error("[MainWindow] createPluginLeftTab called with null plugin");
        return;
    }
    
    if (!m_pluginLeftTabs) {
        util::Logger::Error("[MainWindow] createPluginLeftTab called but m_pluginLeftTabs is null");
        return;
    }
    
    util::Logger::Info("[MainWindow] Creating left/config tab for plugin: {}", pluginId);

    // First, allow SDK-first plugins to provide a left-panel widget via
    // C-ABI (Plugin_CreateLeftPanel). If present, prefer that widget as
    // the left-dock/filter-panel content. Otherwise fall back to the
    // legacy GetConfigurationUI() path which goes into the plugin config dock.
    QWidget* configWidget = nullptr;
    bool addedToLeft = false;
    try {
        auto& pluginManager = plugin::PluginManager::GetInstance();
        const auto& loaded = pluginManager.GetLoadedPlugins();
        auto it = loaded.find(pluginId);
        if (it != loaded.end() && it->second.pluginCreateLeftPanel && it->second.pluginOpaqueHandle) {
            using CreatePanelFn = void*(*)(void*, void*, const char*);
            auto fn = reinterpret_cast<CreatePanelFn>(it->second.pluginCreateLeftPanel);
            void* w = nullptr;
            try { w = fn(it->second.pluginOpaqueHandle, static_cast<void*>(m_pluginLeftTabs), nullptr); } catch(...) { w = nullptr; }
            if (w) {
                configWidget = reinterpret_cast<QWidget*>(w);
                addedToLeft = true;
                util::Logger::Info("[MainWindow] Using plugin-provided left panel for left dock: {}", pluginId);
            }
        }
    } catch (...) { }

    // Fallback to plugin-provided configuration UI via C++ API if no left-panel C-ABI widget
    if (!configWidget) {
        try { configWidget = plugin->GetConfigurationUI(); } catch(...) { configWidget = nullptr; }
    }
    if (!configWidget) {
        util::Logger::Info("[MainWindow] Plugin {} does not provide a configuration UI", pluginId);
        // Create a lightweight host-side placeholder so the Plugin Configuration
        // tab is not empty and offers a simple enable/disable control.
        QWidget* placeholder = new QWidget(m_pluginLeftTabs);
        auto* vlayout = new QVBoxLayout(placeholder);
        vlayout->setContentsMargins(8, 8, 8, 8);
        vlayout->setSpacing(8);
        auto* infoLabel = new QLabel(tr("No configuration UI provided by plugin."), placeholder);
        vlayout->addWidget(infoLabel);
        auto* controlRow = new QWidget(placeholder);
        auto* hlayout = new QHBoxLayout(controlRow);
        hlayout->setContentsMargins(0,0,0,0);
        hlayout->setSpacing(8);
        auto* toggleBtn = new QPushButton(controlRow);
        // Determine current enabled state
        bool enabled = false;
        try {
            auto& pm = plugin::PluginManager::GetInstance();
            const auto& loaded = pm.GetLoadedPlugins();
            auto itp = loaded.find(pluginId);
            if (itp != loaded.end()) enabled = itp->second.enabled;
        } catch(...) {}
        toggleBtn->setText(enabled ? tr("Disable Plugin") : tr("Enable Plugin"));
        hlayout->addWidget(toggleBtn);
        controlRow->setLayout(hlayout);
        vlayout->addWidget(controlRow);

        // Connect button to toggle enable/disable via PluginManager
        connect(toggleBtn, &QPushButton::clicked, this, [pluginId, toggleBtn]() {
            auto& pm = plugin::PluginManager::GetInstance();
            const auto& loaded = pm.GetLoadedPlugins();
            auto it = loaded.find(pluginId);
            if (it != loaded.end()) {
                if (it->second.enabled) {
                    pm.DisablePlugin(pluginId);
                    toggleBtn->setText(tr("Enable Plugin"));
                } else {
                    pm.EnablePlugin(pluginId);
                    toggleBtn->setText(tr("Disable Plugin"));
                }
            }
        });

        configWidget = placeholder;
    }
    
    // Wrap the config widget inside a host-owned container and then a scroll area
    auto metadata = plugin->GetMetadata();
    QString tabName = QString::fromStdString(metadata.name.empty() ? pluginId : metadata.name);

    if (addedToLeft && m_pluginLeftTabs) {
        // Create host container parented to plugin left tabs, then place in scroll area
        QWidget* hostContainer = CreateHostContainerForPluginWidget(configWidget, m_pluginLeftTabs);
        auto* leftScroll = new QScrollArea(m_pluginLeftTabs);
        leftScroll->setWidget(hostContainer);
        leftScroll->setWidgetResizable(true);
        leftScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        leftScroll->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        leftScroll->setFrameShape(QFrame::NoFrame);

        int tabIndex = m_pluginLeftTabs->addTab(leftScroll, tabName);
        m_pluginLeftTabIndices[pluginId] = tabIndex;
        // Ensure left dock is visible
        if (m_pluginLeftDock) {
            m_pluginLeftDock->setFloating(false);
            addDockWidget(Qt::LeftDockWidgetArea, m_pluginLeftDock);
            m_pluginLeftDock->show();
            if (m_filtersDock) tabifyDockWidget(m_filtersDock, m_pluginLeftDock);
        }
        util::Logger::Info("[MainWindow] Added plugin left-panel config tab at index {}: {}", tabIndex, pluginId);
    } else {
        // Default: add to plugin config dock (wrap host container in scroll area)
        QWidget* hostContainer = CreateHostContainerForPluginWidget(configWidget, m_pluginLeftTabs);
        auto* scrollArea = new QScrollArea(m_pluginLeftTabs);
        scrollArea->setWidget(hostContainer);
        scrollArea->setWidgetResizable(true);
        scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
        scrollArea->setFrameShape(QFrame::NoFrame);

        int tabIndex = m_pluginLeftTabs->addTab(scrollArea, tabName);
        m_pluginLeftTabIndices[pluginId] = tabIndex;

        // Show the dock when first config tab is added
        if (m_pluginLeftDock && m_pluginLeftTabs->count() == 1) {
            m_pluginLeftDock->setFloating(false);
            m_pluginLeftDock->show();
            m_pluginLeftDock->raise();
        }

        util::Logger::Info("[MainWindow] Created plugin left/config tab: {} at index {}", tabName.toStdString(), tabIndex);
    }
}

void MainWindow::removePluginLeftTab(const std::string& pluginId) {
    util::Logger::Info("[MainWindow] Removing left/config tab for plugin: {}", pluginId);

    // Try removing from left filter tabs first
    auto fit = m_pluginFilterTabIndices.find(pluginId);
    if (fit != m_pluginFilterTabIndices.end()) {
        int tabIndex = fit->second;
        if (tabIndex >= 0 && m_filterTabs && tabIndex < m_filterTabs->count()) {
            QWidget* widget = m_filterTabs->widget(tabIndex);
            m_filterTabs->removeTab(tabIndex);
            if (widget) widget->deleteLater();
            util::Logger::Info("[MainWindow] Removed plugin left-panel at index {}", tabIndex);
        }
        m_pluginFilterTabIndices.erase(fit);
        for (auto& [id, idx] : m_pluginFilterTabIndices) {
            if (idx > tabIndex) idx--;
        }
    }

    // Also remove from plugin config tabs (fallback)
    auto cit = m_pluginLeftTabIndices.find(pluginId);
    if (cit != m_pluginLeftTabIndices.end()) {
        int tabIndex = cit->second;
        if (tabIndex >= 0 && m_pluginLeftTabs && tabIndex < m_pluginLeftTabs->count()) {
            QWidget* widget = m_pluginLeftTabs->widget(tabIndex);
            m_pluginLeftTabs->removeTab(tabIndex);
            if (widget) widget->deleteLater();
            util::Logger::Info("[MainWindow] Removed plugin config tab at index {}", tabIndex);
        }
        m_pluginLeftTabIndices.erase(cit);
        if (m_pluginLeftDock && m_pluginLeftTabs->count() == 0) {
            m_pluginLeftDock->hide();
        }
        for (auto& [id, idx] : m_pluginLeftTabIndices) {
            if (idx > tabIndex) idx--;
        }
    }
}

void MainWindow::reloadPlugins() {
    if (!m_contentTabs) {
        util::Logger::Warn("[MainWindow] Content tabs not initialized");
        return;
    }
    
    util::Logger::Info("[MainWindow] Reloading plugins...");
    
    // Clear AI-specific UI to let provider refresh cleanly
    RemoveMainPanel();
    RemoveLeftPanel();
    RemoveBottomPanel();
    RemoveRightPanel();

    // Clear plugin tab tracking
    m_pluginTabIndices.clear();
    m_pluginFilterTabIndices.clear();
    
    // Remove all plugin tabs (keep the Events tab at index 0)
    while (m_contentTabs->count() > 1) {
        QWidget* widget = m_contentTabs->widget(1);
        m_contentTabs->removeTab(1);
        if (widget) {
            widget->deleteLater();
        }
    }
    
    // Unload all plugins
    auto& pluginManager = plugin::PluginManager::GetInstance();
    const auto& loadedPlugins = pluginManager.GetLoadedPlugins();
    std::vector<std::string> pluginIds;
    for (const auto& [id, info] : loadedPlugins) {
        pluginIds.push_back(id);
    }
    for (const auto& id : pluginIds) {
        pluginManager.UnloadPlugin(id);
    }
    
    // Reload all plugins (callback will create tabs)
    loadPlugins();
    util::Logger::Info("[MainWindow] Plugins reloaded");
}

void MainWindow::OnSetDarkTheme()
{
    ApplyTheme(*qApp, 0);
    util::Logger::Info("[MainWindow] Dark theme applied");
}

void MainWindow::OnSetLightTheme()
{
    ApplyTheme(*qApp, 1);
    util::Logger::Info("[MainWindow] Light theme applied");
}

void MainWindow::OnSetSystemTheme()
{
    ApplyTheme(*qApp, 2);
    util::Logger::Info("[MainWindow] System theme applied");
}

std::vector<int> MainWindow::GetRowsToExport() const
{
    auto* m = m_eventsView ? m_eventsView->model() : nullptr;
    if (!m) return {};

    // Prefer the current selection if any rows are explicitly selected.
    const QModelIndexList sel =
        m_eventsView->selectionModel() ? m_eventsView->selectionModel()->selectedIndexes()
                                       : QModelIndexList{};

    std::set<int> rowSet;
    for (const auto& idx : sel)
        rowSet.insert(idx.row());

    if (!rowSet.empty())
        return {rowSet.begin(), rowSet.end()};

    // Nothing selected — export every visible (filtered) row.
    const int n = m->rowCount();
    std::vector<int> all;
    all.reserve(static_cast<size_t>(n));
    for (int r = 0; r < n; ++r)
        all.push_back(r);
    return all;
}

void MainWindow::OnExportCsvRequested()
{
    const auto rows = GetRowsToExport();
    if (rows.empty()) {
        QMessageBox::information(this, tr("Export"), tr("No data to export."));
        return;
    }

    QFileDialog dialog(this, tr("Export to CSV"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilter(tr("CSV files (*.csv);;All files (*.*)"));
    dialog.setDefaultSuffix(QStringLiteral("csv"));
#ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
#endif
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString path = dialog.selectedFiles().value(0);
    if (path.isEmpty())
        return;

    if (!ExportManager::ToCsv(*m_eventsView->model(), rows, path)) {
        QMessageBox::warning(this, tr("Export Failed"),
                             tr("Could not write to:\n%1").arg(path));
    }
}

void MainWindow::OnExportJsonRequested()
{
    const auto rows = GetRowsToExport();
    if (rows.empty()) {
        QMessageBox::information(this, tr("Export"), tr("No data to export."));
        return;
    }

    QFileDialog dialog(this, tr("Export to JSON"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilter(tr("JSON files (*.json);;All files (*.*)"));
    dialog.setDefaultSuffix(QStringLiteral("json"));
#ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
#endif
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString path = dialog.selectedFiles().value(0);
    if (path.isEmpty())
        return;

    if (!ExportManager::ToJson(*m_eventsView->model(), rows, path)) {
        QMessageBox::warning(this, tr("Export Failed"),
                             tr("Could not write to:\n%1").arg(path));
    }
}

void MainWindow::OnExportXmlRequested()
{
    const auto rows = GetRowsToExport();
    if (rows.empty()) {
        QMessageBox::information(this, tr("Export"), tr("No data to export."));
        return;
    }

    QFileDialog dialog(this, tr("Export to XML"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilter(tr("XML files (*.xml);;All files (*.*)"));
    dialog.setDefaultSuffix(QStringLiteral("xml"));
#ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
#endif
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString path = dialog.selectedFiles().value(0);
    if (path.isEmpty())
        return;

    if (!ExportManager::ToXml(*m_eventsView->model(), rows, path)) {
        QMessageBox::warning(this, tr("Export Failed"),
                             tr("Could not write to:\n%1").arg(path));
    }
}

// ---------------------------------------------------------------------------
// Update mechanism
// ---------------------------------------------------------------------------

bool MainWindow::ShouldCheckForUpdates() const
{
    const auto& cfg = config::GetConfig().updates;
    if (!cfg.checkOnStartup) return false;
    if (cfg.lastCheckTime.empty()) return true;

    // Parse ISO datetime and compare against interval
    const QDateTime last = QDateTime::fromString(
        QString::fromStdString(cfg.lastCheckTime), Qt::ISODate);
    if (!last.isValid()) return true;

    const int daysSince = static_cast<int>(last.daysTo(QDateTime::currentDateTimeUtc()));
    return daysSince >= cfg.checkIntervalDays;
}

void MainWindow::OnCheckForUpdates()
{
    if (!m_updateChecker) return;

    if (m_updateChecker->IsChecking())
    {
        UpdateStatusText("Checking for updates...");
        return;
    }

    if (m_lastUpdateResult.HasAnyUpdate())
    {
        // Show cached result immediately; user can re-check via the dialog title
        auto* dlg = new UpdateDialog(m_lastUpdateResult, m_updateChecker, this);
        connect(dlg, &UpdateDialog::ApplyPluginUpdate,
                this, &MainWindow::OnApplyPluginUpdate);
        dlg->setAttribute(Qt::WA_DeleteOnClose);
        dlg->exec();
    }
    else
    {
        // Trigger a fresh check and open the dialog when it completes
        connect(m_updateChecker, &UpdateChecker::UpdateCheckComplete,
                this, [this](updates::UpdateCheckResult result) {
                    // Disconnect the one-shot connection by using a single-shot
                    // lambda guard; we rely on Qt auto-disconnect after exec.
                    auto* dlg = new UpdateDialog(result, m_updateChecker, this);
                    connect(dlg, &UpdateDialog::ApplyPluginUpdate,
                            this, &MainWindow::OnApplyPluginUpdate);
                    dlg->setAttribute(Qt::WA_DeleteOnClose);
                    dlg->exec();
                }, static_cast<Qt::ConnectionType>(Qt::SingleShotConnection));
        m_updateChecker->CheckAsync();
        UpdateStatusText("Checking for updates...");
    }
}

void MainWindow::OnUpdateCheckComplete(updates::UpdateCheckResult result)
{
    m_lastUpdateResult = result;

    // Persist the check timestamp
    config::GetConfig().updates.lastCheckTime =
        QDateTime::currentDateTimeUtc().toString(Qt::ISODate).toStdString();
    config::GetConfig().SaveConfig();

    if (result.HasAnyUpdate())
    {
        util::Logger::Info("[MainWindow] Update available — showing badge");
        if (m_updateBadge)
            m_updateBadge->show();
    }
    else
    {
        util::Logger::Info("[MainWindow] No updates available");
        if (m_updateBadge)
            m_updateBadge->hide();
    }

    UpdateStatusText("Ready");
}

void MainWindow::OnApplyPluginUpdate(QString pluginId, QString tempZipPath)
{
    const std::string id = pluginId.toStdString();
    util::Logger::Info("[MainWindow] Applying plugin update: {}", id);

    auto& pm = plugin::PluginManager::GetInstance();

    // 1. Disable (unload) — triggers OnPluginEvent(Disabled) which removes tabs
    auto disResult = pm.DisablePlugin(id);
    if (disResult.isErr())
    {
        util::Logger::Warn("[MainWindow] Could not disable plugin {} before update: {}",
                           id, disResult.error().what());
        // Continue anyway — RegisterPlugin will overwrite files even if loaded
    }

    // 2. Extract/replace the plugin files
    const std::filesystem::path zipPath(tempZipPath.toStdString());
    auto regResult = pm.RegisterPlugin(zipPath);
    if (regResult.isErr())
    {
        const QString errMsg =
            tr("Failed to install plugin update for %1:\n%2")
                .arg(pluginId,
                     QString::fromStdString(regResult.error().what()));
        util::Logger::Error("[MainWindow] {}", errMsg.toStdString());
        QMessageBox::critical(this, tr("Plugin Update Failed"), errMsg);
        return;
    }

    // 3. Re-enable (load new version) — triggers OnPluginEvent(Enabled) which recreates tabs
    auto enableResult = pm.EnablePlugin(id);
    if (enableResult.isErr())
    {
        util::Logger::Warn("[MainWindow] Could not re-enable plugin {} after update: {}",
                           id, enableResult.error().what());
    }

    // 4. Clean up temp file
    std::error_code ec;
    std::filesystem::remove(zipPath, ec);

    util::Logger::Info("[MainWindow] Plugin {} updated successfully", id);
    UpdateStatusText(tr("Plugin %1 updated").arg(pluginId).toStdString());
}

} // namespace ui::qt
