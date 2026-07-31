#include "MainWindow.hpp"
#include "StartupSplash.hpp"

#include "EventsContainer.hpp"
#include "utils/ExportManager.hpp"
#include "utils/FileTailer.hpp"
#include "utils/ReportGenerator.hpp"
#include "FilterManager.hpp"
#include "panels/LayoutManager.hpp"
#include "panels/DashboardPanel.hpp"
#include "panels/StatsSummaryPanel.hpp"
#include "panels/PatternAnalysisPanel.hpp"
#include "panels/SignalPlotPanel.hpp"
#include "panels/TimelineChartPanel.hpp"
#include "panels/TraceViewerPanel.hpp"
#include "panels/SequenceDiagramPanel.hpp"
#include "panels/BookmarksPanel.hpp"
#include "panels/ScenariosPanel.hpp"
#include "panels/SideBySidePanel.hpp"
#include "panels/ActorsPanel.hpp"
#include "panels/ActorDefinitionsPanel.hpp"
#include "panels/SearchBar.hpp"
#include "panels/UnifiedSearchBar.hpp"
#include "utils/UpdateChecker.hpp"
#include "dialogs/UpdateDialog.hpp"
#include "widgets/FilterStatusBar.hpp"
#include "widgets/TabBadgeManager.hpp"
#include "IControler.hpp"
#include "MainWindowPresenter.hpp"
#include "events/EventsTableView.hpp"
#include "panels/FiltersPanel.hpp"
#include "panels/ItemDetailsView.hpp"
#include "panels/SearchResultsView.hpp"
#include "utils/TypeFilterView.hpp"
#include "panels/TimeRangeFilterPanel.hpp"
#include "panels/FilterProfilesPanel.hpp"
#include "panels/CanSignalTreePanel.hpp"
#include "dialogs/LogFileLoadDialog.hpp"
#include "Logger.hpp"
#include "Config.hpp"
#include "dialogs/ConfigEditorDialog.hpp"
#include "dialogs/ExportDialog.hpp"
#include "dialogs/GemmaDownloadDialog.hpp"
#include "dialogs/PluginManagerDialog.hpp"
#include "dialogs/PreferencesDialog.hpp"
#include "dialogs/ShortcutsDialog.hpp"
#include "dialogs/StructuredConfigDialog.hpp"
#include "Version.hpp"
#include "PluginManager.hpp"
#include "utils/ThemeSwitcher.hpp"
#include "asc/AscParser.hpp"
#include "evlog/EvlogParser.hpp"
#include "ParserFactory.hpp"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QFile>
#include <QFileDialog>
#include <QTextStream>
#include <QFileInfo>
#include <QStandardPaths>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QInputDialog>
#include <QLineEdit>
#include <QMenuBar>
#include <QMessageBox>
#include <QMimeData>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QSplitter>
#include <QStackedWidget>
#include <QStatusBar>
#include <QString>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QTimer>
#include <QKeySequence>
#include <QDockWidget>
#include <QShortcut>
#include <QToolTip>
#include <QHelpEvent>
#include <QEventLoop>
#include <QScopeGuard>

#include <climits>
#include <set>
#include <numeric>

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
    const size_t sz = container->Size();
    return static_cast<int>(std::min(sz, static_cast<size_t>(INT_MAX)));
}

static char* PluginEvents_GetEventJsonBridge(void* handle, int index)
{
    if (!handle) return nullptr;
    auto container = static_cast<db::EventsContainer*>(handle);
    try {
        const auto& event = container->GetEvent(static_cast<size_t>(index));
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
    db::EventsContainer& events, StartupSplash* splash, QWidget* parent)
    : QMainWindow(parent), m_splash(splash)
{
    m_events = &events;
    m_layoutManager = std::make_unique<LayoutManager>();
    util::Logger::Info("[MainWindow] Initializing main window");

    auto splashStep = [this](const QString& msg) {
        util::Logger::Debug("[MainWindow] {}", msg.toStdString());
        if (m_splash) m_splash->Step(msg);
    };


    try {
        splashStep(tr("Loading recent files…"));
        LoadRecentFiles();

        splashStep(tr("Building user interface…"));
        InitializeUi(events);

        splashStep(tr("Setting up menus…"));
        SetupMenus();

        splashStep(tr("Initializing controller…"));
        InitializePresenter(controller, events);

        splashStep(tr("Setting up plugin system…"));
        setupPluginManager();

        splashStep(tr("Loading plugins…"));
        loadPlugins();

        splashStep(tr("Restoring window layout…"));
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
        if (m_splash)
            m_splash->Error(tr("Startup error: %1").arg(QString::fromUtf8(ex.what())));
        throw;
    }
}

MainWindow::~MainWindow()
{
    // Stop timers before any member teardown so pending callbacks don't fire
    // against partially-destroyed state.
    if (m_panelRefreshTimer)   m_panelRefreshTimer->stop();
    if (m_searchDebounceTimer) m_searchDebounceTimer->stop();

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

        auto* helpHint = new QLabel(tr("💡 Press Ctrl+H for keyboard shortcuts"));
        helpHint->setStyleSheet("color: #888; font-size: 11px; margin: 0 8px;");
        helpHint->setCursor(Qt::PointingHandCursor);
        connect(helpHint, &QLabel::linkActivated, this, [this]() {
            auto dlg = std::make_unique<ShortcutsDialog>(this);
            dlg->exec();
        });
        statusBar()->addPermanentWidget(helpHint, 0);

        // ===== CENTRAL WIDGET: Main content area with search bar + tabs =====
        auto* centralWidget = new QWidget(this);
        if (!centralWidget) {
            throw std::runtime_error("Failed to create central widget");
        }
        auto* centralLayout = new QVBoxLayout(centralWidget);
        centralLayout->setContentsMargins(0, 0, 0, 0);
        centralLayout->setSpacing(0);

        // Unified search bar (top)
        m_unifiedSearchBar = new UnifiedSearchBar(centralWidget);
        if (!m_unifiedSearchBar) {
            throw std::runtime_error("Failed to create unified search bar");
        }
        centralLayout->addWidget(m_unifiedSearchBar);

        // Content tabs (main area)
        m_contentTabs = new QTabWidget(centralWidget);
        if (!m_contentTabs) {
            throw std::runtime_error("Failed to create content tabs");
        }
        centralLayout->addWidget(m_contentTabs);
        centralWidget->setLayout(centralLayout);

        // Set the container as central widget
        setCentralWidget(centralWidget);
        m_contentTabs->tabBar()->installEventFilter(this);

        // Create tab badge manager for showing activity indicators
        m_tabBadgeManager = new TabBadgeManager(m_contentTabs);

        // Events view tab — QStackedWidget with page 0 = normal view, page 1 = side-by-side
        m_eventsView = new EventsTableView(events, m_contentTabs);
        if (!m_eventsView) {
            throw std::runtime_error("Failed to create events view");
        }

        {
            m_eventsStack = new QStackedWidget(m_contentTabs);

            // Page 0: normal events view
            auto* eventsInner  = new QWidget(m_eventsStack);
            auto* eventsLayout = new QVBoxLayout(eventsInner);
            eventsLayout->setContentsMargins(0, 0, 0, 0);
            eventsLayout->setSpacing(0);
            m_searchBar = new SearchBar(eventsInner);
            m_searchBar->setVisible(false);
            eventsLayout->addWidget(m_searchBar);
            eventsLayout->addWidget(m_eventsView);
            m_eventsStack->addWidget(eventsInner);   // index 0

            // Page 1: side-by-side comparison
            m_sideBySidePanel = new SideBySidePanel(m_eventsStack);
            m_eventsStack->addWidget(m_sideBySidePanel); // index 1

            connect(m_sideBySidePanel, &SideBySidePanel::CloseRequested,
                    this, [this]() { m_eventsStack->setCurrentIndex(0); });

            // ===== Dashboard Tab (First tab) =====
            m_dashboardPanel = new DashboardPanel(m_contentTabs);
            if (!m_dashboardPanel) {
                throw std::runtime_error("Failed to create dashboard panel");
            }
            if (m_events) {
                m_dashboardPanel->SetEventsSource(m_events);
            }
            m_contentTabs->addTab(m_dashboardPanel, "📊 Dashboard");
            m_contentTabs->setTabToolTip(m_contentTabs->count() - 1,
                tr("Overview with file info and quick statistics"));

            // Connect dashboard signals
            connect(m_dashboardPanel, &DashboardPanel::ExportRequested,
                    this, [this]() { OnOpenFileRequested(); });  // Placeholder - could trigger export
            connect(m_dashboardPanel, &DashboardPanel::GenerateReportRequested,
                    this, [this]() {
                        if (m_dashboardPanel) {
                            m_dashboardPanel->RecalculateStats();
                        }
                        OnGenerateReportFromDashboard();
                    });
            connect(m_dashboardPanel, &DashboardPanel::BookmarkCurrentRequested,
                    this, [this]() { /* TODO: Bookmark current event */ });

            // ===== Unified Search Bar Setup =====
            if (m_events) {
                m_unifiedSearchBar->SetEventsSource(m_events);
            }
            if (m_eventsView) {
                m_unifiedSearchBar->SetEventsView(m_eventsView);
            }

            // Connect search bar signals
            connect(m_unifiedSearchBar, &UnifiedSearchBar::SearchChanged,
                    this, [this](const QString& query) {
                        if (m_eventsView) {
                            m_eventsView->SetSearchTerm(query, false);
                        }
                        // Also trigger search operation for SearchResultsView in Tools panel
                        // Use debounce timer to avoid excessive searches during typing
                        if (m_searchDebounceTimer) {
                            m_searchDebounceTimer->start();
                        } else {
                            OnSearchRequested();
                        }
                    });

            connect(m_unifiedSearchBar, &UnifiedSearchBar::FilterRequested,
                    this, [this](const QString& /* field */, const QString& value) {
                        if (m_eventsView) {
                            m_eventsView->SetSearchTerm(value, false);
                            m_eventsView->NavigateToNextMatch();
                        }
                    });

            m_contentTabs->addTab(m_eventsStack, "Events");
            m_contentTabs->setTabToolTip(m_contentTabs->count() - 1,
                tr("Browse, search, and filter log events"));
        }

        // Generic service holder for plugins (AI, analyzer, etc.)
        m_currentService = nullptr;

        setCentralWidget(m_contentTabs);

    // ===== LEFT DOCK: Filters, Actors, Search & Configuration =====
    // Filters dock with tabbed interface for actors, filters, time range, etc.
    m_filtersDock = new QDockWidget("🔧 Filters & Actors", this);
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

    // ── Actor Definitions tab (FIRST - most important for many workflows) ──
    m_actorDefPanel = new ActorDefinitionsPanel(m_filterTabs);
    if (m_events) m_actorDefPanel->SetEventsSource(m_events);
    int actorTabIndex = m_filterTabs->addTab(m_actorDefPanel, "🎭 Actors");
    m_filterTabs->setTabToolTip(actorTabIndex, tr("Define and manage actors, view their definitions and relationships"));

    // ── Extended Filters tab ──────────────────────────────────────────────
    int filterTabIndex = m_filterTabs->addTab(filtersTab, "🔍 Filters");
    m_filterTabs->setTabToolTip(filterTabIndex, tr("Create and manage complex filters with AND/OR/NOT logic"));

    // ── Type Filters tab ──────────────────────────────────────────────────
    int typeTabIndex = m_filterTabs->addTab(typeTab, "📊 Type Filter");
    m_filterTabs->setTabToolTip(typeTabIndex, tr("Filter events by type and apply type-based selections"));

    // ── Time Range Filter tab ─────────────────────────────────────────────
    if (m_events)
    {
        m_timeRangePanel = new TimeRangeFilterPanel(*m_events, m_eventsView, m_filterTabs);
        int timeTabIndex = m_filterTabs->addTab(m_timeRangePanel, tr("⏱️ Time Range"));
        m_filterTabs->setTabToolTip(timeTabIndex, tr("Filter events by date and time range"));
    }

    // ── Filter Profiles tab ───────────────────────────────────────────────
    m_profilesPanel = new FilterProfilesPanel(m_filterTabs);
    int profileTabIndex = m_filterTabs->addTab(m_profilesPanel, tr("💾 Profiles"));
    m_filterTabs->setTabToolTip(profileTabIndex, tr("Save and load filter configurations as reusable profiles"));

    m_filtersDock->setWidget(m_filterTabs);
    addDockWidget(Qt::LeftDockWidgetArea, m_filtersDock);

    // Generic Plugin Configuration dock with tabs for multiple plugins
    m_pluginLeftDock = new QDockWidget("🔌 Plugin Configuration", this);
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

    // ===== LEFT DOCK: Signal Browser (CAN/ASC signals) =====
    m_signalBrowserDock = new QDockWidget(tr("📡 Signal Browser (CAN)"), this);
    m_signalBrowserDock->setObjectName("SignalBrowserDockWidget");
    m_signalBrowserDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_signalBrowserDock->setFeatures(QDockWidget::DockWidgetMovable |
                                     QDockWidget::DockWidgetFloatable |
                                     QDockWidget::DockWidgetClosable);
    m_canSignalTree = new CanSignalTreePanel(m_signalBrowserDock);
    m_signalBrowserDock->setWidget(m_canSignalTree);
    addDockWidget(Qt::LeftDockWidgetArea, m_signalBrowserDock);
    tabifyDockWidget(m_filtersDock, m_signalBrowserDock);

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
    m_contentTabs->setTabToolTip(m_contentTabs->count() - 1,
        tr("Field value distributions, counts, and summary metrics for loaded events"));

    // ===== MAIN TAB: Pattern Analysis =====
    m_patternPanel = new PatternAnalysisPanel(events, m_eventsView, this);
    m_contentTabs->addTab(m_patternPanel, tr("Patterns"));
    m_contentTabs->setTabToolTip(m_contentTabs->count() - 1,
        tr("Detect recurring message patterns and show frequency analysis"));

    // ===== MAIN TAB: Actors =====
    m_actorsPanel = new ActorsPanel(events, m_eventsView, this);
    m_contentTabs->addTab(m_actorsPanel, tr("Actors"));
    m_contentTabs->setTabToolTip(m_contentTabs->count() - 1,
        tr("Events grouped by actor or service, based on actor definitions — click to filter"));

    // ===== MAIN TAB: Timeline (interactive event-distribution chart) =====
    m_timelinePanel = new TimelineChartPanel(events, m_eventsView, this);
    m_contentTabs->addTab(m_timelinePanel, tr("Timeline"));
    m_contentTabs->setTabToolTip(m_contentTabs->count() - 1,
        tr("Event volume over time, colour-coded by log level — click a bar to filter to that time bucket"));

    // ===== MAIN TAB: Signal Plot (CAN signal values over time) =====
    m_signalPlotPanel = new SignalPlotPanel(events, this);
    m_contentTabs->addTab(m_signalPlotPanel, tr("Signals"));
    m_contentTabs->setTabToolTip(m_contentTabs->count() - 1,
        tr("Plot decoded signal values (SIG:*) over time — select signals in the Signal Browser"));

    // Wire Signal Browser → Signal Plot + Events filter.
    connect(m_canSignalTree, &CanSignalTreePanel::SignalSelectionChanged,
            this, [this]() {
                const auto selectedSigs = m_canSignalTree->GetSelectedSignals();
                m_signalPlotPanel->SetSelectedSignals(selectedSigs);

                // Filter the events table to rows that carry at least one
                // selected signal key (stored as "SIG:<name>" in each event).
                if (selectedSigs.empty())
                {
                    m_eventsView->ClearFilter();
                }
                else
                {
                    std::vector<unsigned long> matching;
                    const size_t total = m_events->Size();
                    matching.reserve(total / 4);
                    for (unsigned long i = 0; i < total; ++i)
                    {
                        const auto& ev = m_events->GetEvent(i);
                        for (const auto& sigKey : selectedSigs)
                        {
                            if (!ev.findByKey(sigKey).empty())
                            {
                                matching.push_back(i);
                                break;
                            }
                        }
                    }
                    m_eventsView->SetFilteredEvents(std::move(matching));
                }
            });

    // ===== MAIN TAB: Trace Viewer (group by correlation field) =====
    m_tracePanel = new TraceViewerPanel(events, m_eventsView, this);
    m_contentTabs->addTab(m_tracePanel, tr("Traces"));
    m_contentTabs->setTabToolTip(m_contentTabs->count() - 1,
        tr("Group events by a correlation field (trace ID, session, request…) — double-click a group to filter"));

    // ===== MAIN TAB: Sequence Diagram (auto-discovered from→to actors) =====
    m_sequencePanel = new SequenceDiagramPanel(events, m_eventsView, this);
    m_contentTabs->addTab(m_sequencePanel, tr("Sequence"));
    m_contentTabs->setTabToolTip(m_contentTabs->count() - 1,
        tr("Auto-discovers from/to actor fields and renders a sequence diagram — "
           "double-click an arrow to navigate to that event"));

    // ===== MAIN TAB: Bookmarks (annotate and navigate events) =====
    m_bookmarksPanel = new BookmarksPanel(events, m_eventsView, this);
    m_contentTabs->addTab(m_bookmarksPanel, tr("Bookmarks"));
    m_contentTabs->setTabToolTip(m_contentTabs->count() - 1,
        tr("Annotate events with labels; double-click or Go To to jump back to the event in the Events tab"));

    // ===== MAIN TAB: Scenarios (named event collections for export) =====
    m_scenariosPanel = new ScenariosPanel(events, m_eventsView, this);
    m_contentTabs->addTab(m_scenariosPanel, tr("Scenarios"));
    m_contentTabs->setTabToolTip(m_contentTabs->count() - 1,
        tr("Build named, ordered event collections and export them as plain text, Markdown, or JSON Lines"));

    // ===== UPDATE CHECKER =====
    m_updateChecker = new UpdateChecker(this);
    connect(m_updateChecker, &UpdateChecker::UpdateCheckComplete,
            this,            &MainWindow::OnUpdateCheckComplete);

    // Status bar badge — hidden until an update is found
    m_updateBadge = new QLabel(tr("  ⬆ Update available  "), this);
    m_updateBadge->setStyleSheet(
        "color: white; background-color: #007AFF; border-radius: 3px;"
        " padding: 1px 4px; font-weight: bold;");
    // Add filter status bar to show active filters
    m_filterStatusBar = new FilterStatusBar(this);
    statusBar()->addWidget(m_filterStatusBar);

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
        // Filter is only applied when the user clicks the Apply button.
        // CheckBox changes are intentionally not wired to immediate apply.
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

// ---------------------------------------------------------------------------
// Layout management
// ---------------------------------------------------------------------------

LayoutDescriptor MainWindow::CaptureLayout(const QString& name) const
{
    LayoutDescriptor d;
    d.name               = name;
    d.isBuiltIn          = false;
    d.windowState        = saveState();
    d.filtersDockVisible = m_filtersDock && m_filtersDock->isVisible();
    d.detailsDockVisible = m_detailsDock && m_detailsDock->isVisible();
    d.bottomDockVisible  = m_bottomDock  && m_bottomDock->isVisible();
    if (m_contentTabs) {
        d.activeTab = m_contentTabs->tabText(m_contentTabs->currentIndex());
        for (int i = 0; i < m_contentTabs->count(); ++i)
            d.tabVisibility[m_contentTabs->tabText(i)] =
                m_contentTabs->tabBar()->isTabVisible(i);
    }
    return d;
}

void MainWindow::ApplyLayout(const LayoutDescriptor& layout)
{
    // Restore full dock positions only for user layouts
    if (!layout.isBuiltIn && !layout.windowState.isEmpty())
        restoreState(layout.windowState);

    // Dock visibility
    if (m_filtersDock) m_filtersDock->setVisible(layout.filtersDockVisible);
    if (m_detailsDock) m_detailsDock->setVisible(layout.detailsDockVisible);
    if (m_bottomDock)  m_bottomDock->setVisible(layout.bottomDockVisible);

    // Tab visibility + active tab
    if (m_contentTabs) {
        int activateIdx = -1;
        for (int i = 0; i < m_contentTabs->count(); ++i) {
            const QString label = m_contentTabs->tabText(i);
            const auto it = layout.tabVisibility.find(label);
            if (it != layout.tabVisibility.end())
                m_contentTabs->tabBar()->setTabVisible(i, it.value());
            if (label == layout.activeTab && m_contentTabs->tabBar()->isTabVisible(i))
                activateIdx = i;
        }
        if (activateIdx >= 0) {
            m_contentTabs->setCurrentIndex(activateIdx);
        } else if (!m_contentTabs->tabBar()->isTabVisible(m_contentTabs->currentIndex())) {
            for (int i = 0; i < m_contentTabs->count(); ++i) {
                if (m_contentTabs->tabBar()->isTabVisible(i)) {
                    m_contentTabs->setCurrentIndex(i);
                    break;
                }
            }
        }
    }

    UpdateStatusText(tr("Layout applied: %1").arg(layout.name).toStdString());
}

void MainWindow::RefreshLayoutMenu()
{
    if (!m_layoutsMenu) return;
    m_layoutsMenu->clear();

    auto* saveAction = m_layoutsMenu->addAction(tr("&Save Current Layout…"));
    connect(saveAction, &QAction::triggered, this, &MainWindow::OnSaveLayoutRequested);

    m_layoutsMenu->addSeparator();
    m_layoutsMenu->addSection(tr("Predefined"));

    for (auto& builtIn : LayoutManager::BuiltInLayouts()) {
        auto* action = m_layoutsMenu->addAction(builtIn.name);
        connect(action, &QAction::triggered, this,
            [this, d = std::move(builtIn)]() { ApplyLayout(d); });
    }

    const auto& userLayouts = m_layoutManager->UserLayouts();
    if (!userLayouts.empty()) {
        m_layoutsMenu->addSeparator();
        m_layoutsMenu->addSection(tr("Saved"));
        for (const auto& d : userLayouts) {
            auto* sub         = m_layoutsMenu->addMenu(d.name);
            auto* applyAction  = sub->addAction(tr("Apply"));
            auto* deleteAction = sub->addAction(tr("Delete"));
            connect(applyAction,  &QAction::triggered, this,
                [this, copy = d]() { ApplyLayout(copy); });
            connect(deleteAction, &QAction::triggered, this,
                [this, name = d.name]() { OnDeleteLayoutRequested(name); });
        }
    }
}

void MainWindow::OnSaveLayoutRequested()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Save Layout"), tr("Layout name:"),
        QLineEdit::Normal, QString{}, &ok);
    if (!ok || name.trimmed().isEmpty()) return;

    m_layoutManager->Save(CaptureLayout(name.trimmed()));
    RefreshLayoutMenu();
    UpdateStatusText(tr("Layout saved: %1").arg(name).toStdString());
}

void MainWindow::OnDeleteLayoutRequested(const QString& name)
{
    const auto btn = QMessageBox::question(
        this, tr("Delete Layout"),
        tr("Delete layout \"%1\"?").arg(name));
    if (btn != QMessageBox::Yes) return;
    m_layoutManager->Remove(name);
    RefreshLayoutMenu();
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
    openAction->setToolTip(tr("Open a log file (CSV, JSON, XML, ASC, Evlog)"));
    connect(openAction, &QAction::triggered, this, [this]() {
        util::Logger::Debug("[MainWindow] Open menu triggered");
        OnOpenFileRequested();
    });

    // Add Recent Files submenu
    m_recentFilesMenu = fileMenu->addMenu(tr("Recent &Files"));
    m_recentFilesMenu->setEnabled(!m_recentFiles.empty());
    RefreshRecentFilesMenu();

    fileMenu->addSeparator();

    auto* openSessionAction = fileMenu->addAction(tr("Open Session…"));
    openSessionAction->setShortcut(QKeySequence(tr("Ctrl+Shift+O")));
    openSessionAction->setToolTip(tr("Load a saved analysis session with filters and layout"));
    connect(openSessionAction, &QAction::triggered, this, &MainWindow::OnOpenSession);

    auto* saveSessionAction = fileMenu->addAction(tr("Save Session…"));
    saveSessionAction->setShortcut(QKeySequence(tr("Ctrl+Shift+S")));
    saveSessionAction->setToolTip(tr("Save current filters, layout, and analysis state"));
    connect(saveSessionAction, &QAction::triggered, this, &MainWindow::OnSaveSession);

    fileMenu->addSeparator();

    auto* loadDbcAction = fileMenu->addAction(tr("Load &DBC file…"));
    loadDbcAction->setToolTip(tr("Load a DBC file for CAN signal name translation"));
    connect(loadDbcAction, &QAction::triggered, this, &MainWindow::OnLoadDbcRequested);

    auto* loadEvlogTplAction = fileMenu->addAction(tr("Load &Evlog Templates…"));
    loadEvlogTplAction->setToolTip(tr("Load Evlog message templates for parsing"));
    connect(loadEvlogTplAction, &QAction::triggered,
        this, &MainWindow::OnLoadEvlogTemplatesRequested);

    fileMenu->addSeparator();

    auto* exportAction = fileMenu->addAction(tr("E&xport..."));
    exportAction->setShortcut(QKeySequence(tr("Ctrl+E")));
    exportAction->setToolTip(tr("Export logs to CSV, JSON, XML, Markdown, HTML, or TSV format"));
    connect(exportAction, &QAction::triggered, this, [this]() {
        if (!m_events) return;
        auto dialog = std::make_unique<ExportDialog>(*m_events, m_eventsView, this);
        dialog->exec();
    });

    fileMenu->addSeparator();

    auto* clearAction = fileMenu->addAction(tr("&Clear Data"));
    clearAction->setShortcut(QKeySequence(tr("Ctrl+Shift+L")));
    connect(clearAction, &QAction::triggered, this,
        &MainWindow::OnClearDataRequested);

    m_tailAction = fileMenu->addAction(tr("&Follow File (Tail)"));
    m_tailAction->setShortcut(QKeySequence(tr("Ctrl+T")));
    m_tailAction->setCheckable(true);
    m_tailAction->setEnabled(false);
    m_tailAction->setToolTip(tr("Watch the current log file and automatically load new lines as they are appended.\n"
                                "Supported formats: NDJSON (.json/.jsonl), CAN/ASC, DLT.\n"
                                "Not supported: XML, CSV."));
    connect(m_tailAction, &QAction::triggered, this, &MainWindow::OnToggleTailRequested);

    fileMenu->addSeparator();

    auto* exitAction = fileMenu->addAction(tr("E&xit"));
    exitAction->setShortcut(QKeySequence::Quit);
    connect(exitAction, &QAction::triggered, this,
        &MainWindow::OnExitRequested);

    auto* toolsMenu = bar->addMenu(tr("&Tools"));

    auto* preferencesAction = toolsMenu->addAction(tr("&Preferences..."));
    preferencesAction->setShortcut(QKeySequence::Preferences);
    connect(preferencesAction, &QAction::triggered, this, [this]() {
        ui::qt::PreferencesDialog dlg(config::GetConfig(), this);
        dlg.exec();
    });

    toolsMenu->addSeparator();

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
        UpdateStatusText("Reloading plugins...");
        reloadPlugins();
        UpdateStatusText("Plugins reloaded");
        QMessageBox::information(this, tr("Plugins"), tr("Plugins reloaded successfully"));
    });

    auto* managePluginsAction = toolsMenu->addAction(tr("&Manage Plugins…"));
    managePluginsAction->setToolTip(
        tr("Browse, install, enable, disable, and uninstall plugins"));
    connect(managePluginsAction, &QAction::triggered, this, [this]() {
        PluginManagerDialog dlg(this);
        dlg.exec();
    });

    toolsMenu->addSeparator();

    auto* downloadAIModelAction = toolsMenu->addAction(tr("Download AI &Model..."));
    downloadAIModelAction->setToolTip(
        tr("Download a local LLM model (Gemma 2B or other GGUF models) for AI-assisted analysis"));
    connect(downloadAIModelAction, &QAction::triggered, this, [this]() {
        GemmaDownloadDialog dlg(this);
        dlg.exec();
    });

    // Analysis menu — new v1.10.0 features
    auto* analysisMenu = bar->addMenu(tr("&Analysis"));

    auto* generateReportAction = analysisMenu->addAction(tr("Generate &Report..."));
    generateReportAction->setShortcut(QKeySequence(tr("Ctrl+Alt+R")));
    generateReportAction->setToolTip(tr("Generate comprehensive HTML/Markdown/JSON analysis reports"));
    connect(generateReportAction, &QAction::triggered, this, [this]() {
        if (m_events && m_eventsView) {
            // TODO: Launch ReportGenerator dialog here
            QMessageBox::information(this, tr("Report Generator"),
                tr("Report generation feature coming soon.\nUse Export for detailed data export."));
        }
    });

    analysisMenu->addSeparator();

    auto* groupEventsAction = analysisMenu->addAction(tr("&Group Events..."));
    groupEventsAction->setShortcut(QKeySequence(tr("Ctrl+Alt+G")));
    groupEventsAction->setToolTip(tr("Group events by level, message, actor, or time bucket"));
    connect(groupEventsAction, &QAction::triggered, this, [this]() {
        if (m_events && m_eventsView) {
            // TODO: Launch EventGroupManager dialog here
            QMessageBox::information(this, tr("Event Grouping"),
                tr("Event grouping feature is available in the panels.\nTry using different event analysis views."));
        }
    });

    auto* tagEventsAction = analysisMenu->addAction(tr("&Tag & Annotate..."));
    tagEventsAction->setShortcut(QKeySequence(tr("Ctrl+Alt+T")));
    tagEventsAction->setToolTip(tr("Add tags and annotations to events"));
    connect(tagEventsAction, &QAction::triggered, this, [this]() {
        if (m_events && m_eventsView) {
            // TODO: Launch EventTagManager dialog here
            QMessageBox::information(this, tr("Event Tagging"),
                tr("Event tagging feature is available in the event details panel.\nRight-click on events to add tags."));
        }
    });

    // View menu for dock widgets
    auto* viewMenu = bar->addMenu(tr("&View"));

    // Panels submenu — organize dock widgets with descriptions
    auto* panelsMenu = viewMenu->addMenu(tr("&Panels"));

    auto* filtersAction = m_filtersDock->toggleViewAction();
    filtersAction->setText(tr("&Filters & Search"));
    panelsMenu->addAction(filtersAction);

    auto* signalAction = m_signalBrowserDock->toggleViewAction();
    signalAction->setText(tr("&Signal Browser"));
    panelsMenu->addAction(signalAction);

    auto* pluginAction = m_pluginLeftDock->toggleViewAction();
    pluginAction->setText(tr("P&lugin Configuration"));
    panelsMenu->addAction(pluginAction);

    auto* detailsAction = m_detailsDock->toggleViewAction();
    detailsAction->setText(tr("&Event Details (Right)"));
    panelsMenu->addAction(detailsAction);

    auto* toolsAction = m_bottomDock->toggleViewAction();
    toolsAction->setText(tr("&Tools (Bottom)"));
    panelsMenu->addAction(toolsAction);

    viewMenu->addSeparator();

    auto* sideBySideAction = viewMenu->addAction(tr("&Side by Side Comparison"));
    connect(sideBySideAction, &QAction::triggered, this, [this]() {
        ActivateSideBySide();
    });

    viewMenu->addSeparator();

    auto* bookmarksAction = viewMenu->addAction(tr("Show &Bookmarks"));
    bookmarksAction->setShortcut(QKeySequence(tr("Ctrl+B")));
    bookmarksAction->setToolTip(tr("Organize and categorize bookmarked events for quick reference"));
    connect(bookmarksAction, &QAction::triggered, this, [this]() {
        if (!m_bookmarksPanel || !m_contentTabs)
            return;
        // Find the index of the bookmarks tab
        for (int i = 0; i < m_contentTabs->count(); ++i)
        {
            if (m_contentTabs->widget(i) == m_bookmarksPanel)
            {
                // Make tab visible and switch to it
                m_contentTabs->tabBar()->setTabVisible(i, true);
                m_contentTabs->setCurrentIndex(i);
                m_bookmarksPanel->setFocus();
                break;
            }
        }
    });

    auto* jumpAction = viewMenu->addAction(tr("Go to &Timestamp…"));
    jumpAction->setShortcut(QKeySequence(tr("Ctrl+G")));
    jumpAction->setToolTip(tr("Jump to a specific timestamp in the event log"));
    connect(jumpAction, &QAction::triggered, this, [this]() {
        if (m_eventsView) m_eventsView->JumpToTimestamp();
    });

    viewMenu->addSeparator();

    // Tabs submenu — one checkable action per built-in content tab
    auto* tabsMenu = viewMenu->addMenu(tr("&Tabs"));
    for (int i = 0; i < m_contentTabs->count(); ++i)
    {
        auto* action = tabsMenu->addAction(m_contentTabs->tabText(i));
        action->setCheckable(true);
        action->setChecked(true);
        action->setToolTip(m_contentTabs->tabToolTip(i));
        const int idx = i;
        connect(action, &QAction::toggled, this, [this, idx](bool visible) {
            if (m_contentTabs)
                m_contentTabs->tabBar()->setTabVisible(idx, visible);
        });
    }

    viewMenu->addSeparator();

    // Layouts submenu — predefined and user-saved layouts
    m_layoutsMenu = viewMenu->addMenu(tr("&Layouts"));
    RefreshLayoutMenu();

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

    auto* focusSearchAction = viewMenu->addAction(tr("&Find Events (Ctrl+F)"));
    focusSearchAction->setShortcut(QKeySequence::Find);
    focusSearchAction->setToolTip(tr("Focus the search bar and search across all event fields"));
    connect(focusSearchAction, &QAction::triggered, this, [this]() {
        if (m_unifiedSearchBar) {
            m_unifiedSearchBar->FocusSearchInput();
        }
    });

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
        UpdateStatusText("Layout reset");
    });

    // Help menu
    auto* helpMenu = bar->addMenu(tr("&Help"));

    auto* shortcutsAction = helpMenu->addAction(tr("Keyboard &Shortcuts"));
    shortcutsAction->setShortcut(QKeySequence::HelpContents);
    connect(shortcutsAction, &QAction::triggered, this, [this]() {
        ui::qt::ShortcutsDialog dlg(this);
        dlg.exec();
    });

    helpMenu->addSeparator();

    auto* checkUpdatesAction = helpMenu->addAction(tr("Check for &Updates..."));
    connect(checkUpdatesAction, &QAction::triggered, this,
            &MainWindow::OnCheckForUpdates);
    auto* reportIssueAction = helpMenu->addAction(tr("&Report an Issue..."));
    connect(reportIssueAction, &QAction::triggered, this, []() {
        QDesktopServices::openUrl(
            QUrl(QStringLiteral("https://github.com/Nerdbergev/LogViewer/issues/new")));
    });
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

    // Lazy analysis-panel refresh ─────────────────────────────────────────
    // Model resets (filter / load) mark all analysis panels dirty and start a
    // 150 ms debounce timer.  Only the currently visible tab is refreshed when
    // the timer fires or when the user switches tabs.  This prevents cascading
    // 5× O(n) recomputes on every filter change and keeps the UI responsive.
    m_panelRefreshTimer = new QTimer(this);
    m_panelRefreshTimer->setSingleShot(true);
    m_panelRefreshTimer->setInterval(150);
    connect(m_panelRefreshTimer, &QTimer::timeout,
            this, &MainWindow::RefreshCurrentAnalysisPanel);

    if (m_eventsView && m_eventsView->model()) {
        connect(m_eventsView->model(), &QAbstractItemModel::modelReset,
                this, &MainWindow::MarkAnalysisPanelsDirty);
    }

    // Tab switch → refresh the newly-visible panel if its data is stale
    if (m_contentTabs) {
        connect(m_contentTabs, &QTabWidget::currentChanged,
                this, [this](int) { RefreshCurrentAnalysisPanel(); });
    }

    // Connect actor definitions → actors panel + sequence diagram
    if (m_actorDefPanel && m_actorsPanel) {
        connect(m_actorDefPanel, &ActorDefinitionsPanel::DefinitionsChanged,
                m_actorsPanel,   &ActorsPanel::SetDefinitions);
        // Push any already-loaded definitions immediately
        m_actorsPanel->SetDefinitions(m_actorDefPanel->Definitions());
    }
    if (m_actorDefPanel && m_sequencePanel) {
        connect(m_actorDefPanel, &ActorDefinitionsPanel::DefinitionsChanged,
                m_sequencePanel, &SequenceDiagramPanel::SetDefinitions);
        m_sequencePanel->SetDefinitions(m_actorDefPanel->Definitions());
    }

    // Back-channel: actor direction changes made via context menu in Actors panel
    // are forwarded to ActorDefinitionsPanel which persists them.
    if (m_actorsPanel && m_actorDefPanel) {
        connect(m_actorsPanel, &ActorsPanel::ActorDirectionChanged,
                m_actorDefPanel, &ActorDefinitionsPanel::UpdateActorDirection);
    }

    // Auto-switch left panel when a content tab is selected.
    // Actors / Sequence → Actor Definitions tab
    // Signals           → Signal Browser dock (raised + shown)
    if (m_contentTabs && m_filterTabs)
    {
        connect(m_contentTabs, &QTabWidget::currentChanged,
                this, [this](int /*index*/) {
                    QWidget* cur = m_contentTabs->currentWidget();
                    if ((cur == m_actorsPanel || cur == m_sequencePanel) && m_actorDefPanel)
                    {
                        m_filtersDock->show();
                        m_filtersDock->raise();
                        m_filterTabs->setCurrentWidget(m_actorDefPanel);
                    }
                    else if (cur == m_signalPlotPanel && m_signalBrowserDock)
                    {
                        m_signalBrowserDock->show();
                        m_signalBrowserDock->raise();
                    }
                });
    }

    // Time range filter → mark all analysis panels dirty (same debounce path)
    if (m_timeRangePanel) {
        connect(m_timeRangePanel, &TimeRangeFilterPanel::FilterApplied,
                this, &MainWindow::MarkAnalysisPanelsDirty);
        connect(m_timeRangePanel, &TimeRangeFilterPanel::FilterCleared,
                this, &MainWindow::MarkAnalysisPanelsDirty);
    }

    // Bookmarks: right-click in events table → add bookmark; activate bookmark → switch tab + scroll
    if (m_eventsView && m_bookmarksPanel) {
        connect(m_eventsView, &EventsTableView::BookmarkRequested,
                m_bookmarksPanel, &BookmarksPanel::AddBookmarkForRow);

        connect(m_bookmarksPanel, &BookmarksPanel::NavigateToEvent,
                this, [this](int actualRow) {
                    if (m_contentTabs)
                        m_contentTabs->setCurrentIndex(0);
                    m_eventsView->ScrollToActualRow(actualRow);
                });
    }

    // Scenarios: right-click in events table → add event to active scenario
    if (m_eventsView && m_scenariosPanel) {
        connect(m_eventsView, &EventsTableView::AddToScenarioRequested,
                m_scenariosPanel, &ScenariosPanel::AddEventFromRow);
    }

    // Filter profiles
    if (m_profilesPanel) {
        connect(m_profilesPanel, &FilterProfilesPanel::SaveRequested,
                this, &MainWindow::OnProfileSaveRequested);
        connect(m_profilesPanel, &FilterProfilesPanel::ProfileLoadRequested,
                this, &MainWindow::OnProfileLoadRequested);
    }

    // Connect actor definition panel filter buttons
    if (m_actorDefPanel && m_eventsView) {
        connect(m_actorDefPanel, &ActorDefinitionsPanel::RequestApplyFilter,
                this, &MainWindow::ApplyActorFilter);
        connect(m_actorDefPanel, &ActorDefinitionsPanel::RequestClearFilter,
                this, [this]() {
                    m_eventsView->ClearFilter();
                    UpdateStatusText("Actor filter cleared");
                });
    }

    // Search bar (Ctrl+F)
    if (m_searchBar && m_eventsView) {
        // Debounce: buffer 150 ms of keystrokes before the O(n×m) match rebuild.
        m_searchDebounceTimer = new QTimer(this);
        m_searchDebounceTimer->setSingleShot(true);
        m_searchDebounceTimer->setInterval(150);
        connect(m_searchDebounceTimer, &QTimer::timeout, this, [this]() {
            if (m_eventsView)
                m_eventsView->SetSearchTerm(m_pendingSearchTerm, m_pendingSearchCase);
        });
        connect(m_searchBar, &SearchBar::SearchChanged,
                this, [this](const QString& term, bool cs) {
                    m_pendingSearchTerm = term;
                    m_pendingSearchCase = cs;
                    m_searchDebounceTimer->start();
                });
        connect(m_searchBar, &SearchBar::NavigatePrev,
                m_eventsView, &EventsTableView::NavigateToPrevMatch);
        connect(m_searchBar, &SearchBar::NavigateNext,
                m_eventsView, &EventsTableView::NavigateToNextMatch);
        // Close bar → clear highlights
        connect(m_searchBar, &SearchBar::Closed, m_eventsView, [this]() {
            m_eventsView->SetSearchTerm(QString(), false);
            m_searchBar->SetMatchInfo(0, 0);
        });
        // View reports match position → update counter label
        connect(m_eventsView, &EventsTableView::MatchInfoChanged,
                m_searchBar,  &SearchBar::SetMatchInfo);

        // Note: Ctrl+F is now handled by UnifiedSearchBar global shortcut in SetupMenus()
        // The old SearchBar inline search is still available but not via keyboard shortcut
    }

    // Schedule automatic update check with a long startup delay.
    // A short delay (< 5 s) after process start followed by an outbound
    // network connection matches the staged-dropper sandbox-evasion heuristic
    // (process waits for sandbox timeout, then connects). 30 s is well past
    // most AV sandbox run limits while still checking promptly for the user.
    if (m_updateChecker && ShouldCheckForUpdates())
    {
        // Notify the user via message box when the startup check finds updates.
        // Single-shot so only fires once per startup (the permanent
        // OnUpdateCheckComplete still runs to update the badge).
        connect(m_updateChecker, &UpdateChecker::UpdateCheckComplete,
                this, [this](updates::UpdateCheckResult result) {
                    if (!result.HasAnyUpdate()) return;

                    // Build a concise summary for the message box.
                    QStringList lines;
                    if (result.app.isNewer)
                        lines << tr("• App update: v%1 available")
                                     .arg(QString::fromStdString(result.app.version));
                    int compatPlugins = 0;
                    for (const auto& p : result.plugins)
                        if (p.isCompatible) ++compatPlugins;
                    if (compatPlugins > 0)
                        lines << tr("• %1 plugin update(s) available").arg(compatPlugins);

                    QMessageBox mb(this);
                    mb.setWindowTitle(tr("Update Available"));
                    mb.setIcon(QMessageBox::Information);
                    mb.setText(tr("<b>A LogViewer update is available.</b>"));
                    mb.setInformativeText(lines.join('\n'));
                    mb.addButton(tr("View Details…"), QMessageBox::AcceptRole);
                    auto* okBtn = mb.addButton(QMessageBox::Ok);
                    mb.setDefaultButton(okBtn);

                    mb.exec();
                    if (mb.clickedButton() != okBtn)
                    {
                        auto* dlg = new UpdateDialog(result, m_updateChecker, this);
                        connect(dlg, &UpdateDialog::ApplyPluginUpdate,
                                this, &MainWindow::OnApplyPluginUpdate);
                        dlg->setAttribute(Qt::WA_DeleteOnClose);
                        dlg->exec();
                    }
                }, static_cast<Qt::ConnectionType>(Qt::SingleShotConnection));
        QTimer::singleShot(30000, m_updateChecker, &UpdateChecker::CheckAsync);
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
    // Try unified search bar first (top of window - primary search interface)
    if (m_unifiedSearchBar) {
        QString query = m_unifiedSearchBar->GetSearchQuery();
        if (!query.isEmpty())
            return query.toStdString();
    }

    // Fall back to old search edit in Tools panel (bottom)
    if (m_searchEdit)
        return m_searchEdit->text().toStdString();

    return "";
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

std::string MainWindow::AskString(const std::string& title,
    const std::string& prompt, const std::string& defaultValue, bool& ok)
{
    const QString result = QInputDialog::getText(
        this,
        QString::fromStdString(title),
        QString::fromStdString(prompt),
        QLineEdit::Normal,
        QString::fromStdString(defaultValue),
        &ok);
    return ok ? result.toStdString() : std::string{};
}

void MainWindow::UpdateFilterStatus(int totalEvents, int filteredCount,
    int activeFilterCount, const std::string& filterDetails)
{
    if (!m_filterStatusBar)
        return;

    if (activeFilterCount <= 0)
    {
        m_filterStatusBar->ClearStatus();
    }
    else
    {
        m_filterStatusBar->UpdateFilterStatus(totalEvents, filteredCount,
            activeFilterCount, QString::fromStdString(filterDetails));
    }
}

void MainWindow::OnSearchResultActivated(long eventId)
{
        util::Logger::Debug("[MainWindow] OnSearchResultActivated eventId={}",
            eventId);
        for (size_t i = 0; i < m_events->Size(); ++i)
        {
            if (m_events->GetEvent(i).getId() == eventId)
            {
                util::Logger::Debug("[MainWindow] Matching event found at index={}", i);
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

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    if (m_contentTabs && watched == m_contentTabs->tabBar()
            && event->type() == QEvent::ToolTip)
    {
        const auto* he = static_cast<const QHelpEvent*>(event);
        const int idx = m_contentTabs->tabBar()->tabAt(he->pos());
        if (idx >= 0)
            QToolTip::showText(he->globalPos(), m_contentTabs->tabToolTip(idx));
        else
            QToolTip::hideText();
        return true;
    }
    return QMainWindow::eventFilter(watched, event);
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

    QStringList localFiles;
    for (const auto& url : event->mimeData()->urls())
        if (url.isLocalFile())
            localFiles.append(url.toLocalFile());

    if (localFiles.isEmpty())
        return;

    if (localFiles.size() >= 2)
    {
        // Two files dropped: open directly in side-by-side panel.
        util::Logger::Info("[MainWindow] Two files dropped — opening side by side");
        ActivateSideBySide();
        m_sideBySidePanel->OpenLeft(localFiles[0]);
        m_sideBySidePanel->OpenRight(localFiles[1]);
    }
    else
    {
        util::Logger::Info("[MainWindow] Dropped file: {}",
            localFiles[0].toStdString());
        HandleDroppedFile(localFiles[0]);
    }

    event->acceptProposedAction();
}

void MainWindow::AutoSwitchViewForFile(const QString& filePath)
{
    if (!m_contentTabs || filePath.isEmpty())
        return;

    QFileInfo fileInfo(filePath);
    QString ext = fileInfo.suffix().toLower();

    int targetTabIndex = -1;

    // Match file extension to appropriate view tab
    if (ext == "xml")
    {
        // Find XML View tab (if it exists as a specific tab)
        // For now, Events tab shows parsed content
        for (int i = 0; i < m_contentTabs->count(); ++i)
        {
            QString tabText = m_contentTabs->tabText(i);
            if (tabText.contains("Events", Qt::CaseInsensitive) || tabText.contains("Table", Qt::CaseInsensitive))
            {
                targetTabIndex = i;
                break;
            }
        }
    }
    else if (ext == "json" || ext == "jsonl")
    {
        // JSON files → Events tab
        for (int i = 0; i < m_contentTabs->count(); ++i)
        {
            QString tabText = m_contentTabs->tabText(i);
            if (tabText.contains("Events", Qt::CaseInsensitive))
            {
                targetTabIndex = i;
                break;
            }
        }
    }
    else if (ext == "csv")
    {
        // CSV files → Events tab
        for (int i = 0; i < m_contentTabs->count(); ++i)
        {
            QString tabText = m_contentTabs->tabText(i);
            if (tabText.contains("Events", Qt::CaseInsensitive))
            {
                targetTabIndex = i;
                break;
            }
        }
    }
    else if (ext == "asc" || ext == "dbc" || ext == "can")
    {
        // CAN/ASC files → Signal Browser
        for (int i = 0; i < m_contentTabs->count(); ++i)
        {
            QString tabText = m_contentTabs->tabText(i);
            if (tabText.contains("Signal", Qt::CaseInsensitive) || tabText.contains("CAN", Qt::CaseInsensitive))
            {
                targetTabIndex = i;
                break;
            }
        }
        // Fallback to Events if Signal tab not found
        if (targetTabIndex == -1)
        {
            for (int i = 0; i < m_contentTabs->count(); ++i)
            {
                if (m_contentTabs->tabText(i).contains("Events", Qt::CaseInsensitive))
                {
                    targetTabIndex = i;
                    break;
                }
            }
        }
    }
    else if (ext == "dlt")
    {
        // DLT logs → Events tab
        for (int i = 0; i < m_contentTabs->count(); ++i)
        {
            if (m_contentTabs->tabText(i).contains("Events", Qt::CaseInsensitive))
            {
                targetTabIndex = i;
                break;
            }
        }
    }
    else if (ext == "evl")
    {
        // Evlog binary → Events tab
        for (int i = 0; i < m_contentTabs->count(); ++i)
        {
            if (m_contentTabs->tabText(i).contains("Events", Qt::CaseInsensitive))
            {
                targetTabIndex = i;
                break;
            }
        }
    }

    // Switch to the target tab if found
    if (targetTabIndex >= 0 && targetTabIndex < m_contentTabs->count())
    {
        m_contentTabs->setCurrentIndex(targetTabIndex);
        util::Logger::Debug("[MainWindow] Auto-switched to tab {} for file: {}",
            targetTabIndex, filePath.toStdString());
    }
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
                    auto parser = CreateParserFor(filePath);
                    if (!parser) return; // user cancelled type selection
                    const QString message = QString("Loading %1 ...").arg(path);
                    UpdateStatusText(message.toStdString());
                    m_presenter->LoadLogFile(std::move(parser), filePath);
                    m_presenter->SetItemDetailsVisible(true);
                    m_currentLogFilePath = path;
                    if (m_tailAction) m_tailAction->setEnabled(true);
                    AutoSwitchViewForFile(path);
                    const QString readyMsg = QString("Data ready. Path: %1").arg(path);
                    UpdateStatusText(readyMsg.toStdString());
                    AddToRecentFiles(path);
                }
                else if (dialog.GetLoadMode() == LogFileLoadDialog::LoadMode::Merge)
                {
                    const std::string existingAlias = dialog.GetExistingFileAlias().toStdString();
                    const std::string newFileAlias = dialog.GetNewFileAlias().toStdString();
                    const QString mergingMsg = QString("Merging %1 ...").arg(path);
                    UpdateStatusText(mergingMsg.toStdString());
                    m_presenter->MergeLogFile(filePath, existingAlias, newFileAlias);
                    m_presenter->SetItemDetailsVisible(true);
                    const QString completeMsg = QString("Merge complete. Path: %1").arg(path);
                    UpdateStatusText(completeMsg.toStdString());
                    AddToRecentFiles(path);
                }
                else // SideBySide
                {
                    ActivateSideBySide();
                    if (!m_currentLogFilePath.isEmpty())
                        m_sideBySidePanel->OpenLeft(m_currentLogFilePath);
                    m_sideBySidePanel->OpenRight(path);
                    AddToRecentFiles(path);
                }
            }
            // If dialog was canceled, do nothing
        }
        else
        {
            // No existing data, just load normally
            auto parser = CreateParserFor(filePath);
            if (!parser) return; // user cancelled type selection
            const QString message = QString("Loading %1 ...").arg(path);
            UpdateStatusText(message.toStdString());
            m_presenter->LoadLogFile(std::move(parser), filePath);
            m_presenter->SetItemDetailsVisible(true);
            m_currentLogFilePath = path;
            if (m_tailAction) m_tailAction->setEnabled(true);
            AutoSwitchViewForFile(path);
            const QString readyMsg = QString("Data ready. Path: %1").arg(path);
            UpdateStatusText(readyMsg.toStdString());
            AddToRecentFiles(path);
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
    dialog.setNameFilter(tr("Log files (*.log *.txt *.xml *.csv *.json *.jsonl *.asc *.dlt *.evl);;JSON logs (*.json *.jsonl);;CAN ASC logs (*.asc);;AUTOSAR DLT logs (*.dlt);;Evlog binary (*.evl);;All files (*.*)"));
    dialog.setDirectory(LastDir("logFile",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)));
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

    SaveLastDir("logFile", filePath);
    util::Logger::Info("[MainWindow] OnOpenFileRequested path={}",
        filePath.toStdString());

    HandleDroppedFile(filePath);
}

std::unique_ptr<parser::IDataParser> MainWindow::CreateParserFor(
    const std::filesystem::path& path)
{
    std::string ext = path.extension().string();
    std::transform(ext.begin(), ext.end(), ext.begin(),
        [](unsigned char c){ return static_cast<char>(std::tolower(c)); });

    // Unknown or missing extension → ask the user.
    const bool isKnown = (ext == ".asc" || ext == ".evl"
                          || parser::ParserFactory::IsRegistered(ext));
    if (!isKnown)
    {
        const QString chosen = PromptForFileType(path);
        if (chosen.isEmpty())
            return nullptr;  // user cancelled
        ext = chosen.toStdString();
    }

    if (ext == ".asc")
    {
        std::filesystem::path dbcPath;
        if (!m_currentDbcFilePath.isEmpty())
            dbcPath = m_currentDbcFilePath.toStdString();
        return std::make_unique<parser::AscParser>(dbcPath);
    }

    if (ext == ".evl")
    {
        auto parser = std::make_unique<parser::EvlogParser>();
        if (!m_evlogTemplateDir.isEmpty()) {
            auto tmplDirResult = parser->SetTemplateDirectory(m_evlogTemplateDir.toStdString());
            if (tmplDirResult.isErr()) {
                QMessageBox::warning(this, tr("Evlog Templates"),
                    tr("Could not load evlog templates from:\n%1\n\nPayloads will be shown as hex dumps.\n\nError: %2")
                        .arg(m_evlogTemplateDir)
                        .arg(QString::fromStdString(tmplDirResult.error().what())));
            }
        }
        return parser;
    }

    // Factory handles .xml, .csv, .dlt and any plugin-registered extensions.
    // Use path with the (possibly user-chosen) extension so the factory can dispatch.
    const std::filesystem::path effectivePath = path.parent_path() / (path.stem().string() + ext);
    auto result = parser::ParserFactory::CreateFromFile(effectivePath);
    if (result.isOk())
        return result.unwrap();
    throw result.error();
}

QString MainWindow::PromptForFileType(const std::filesystem::path& path)
{
    struct FileType { QString label; QString ext; };
    static const std::array<FileType, 5> kTypes = {{
        { tr("XML"),                          ".xml" },
        { tr("CSV"),                          ".csv" },
        { tr("CAN/ASC (Vector CANalyzer)"),   ".asc" },
        { tr("AUTOSAR DLT"),                  ".dlt" },
        { tr("Evlog binary (POSIX 1003.25)"), ".evl" },
    }};

    QStringList labels;
    for (const auto& t : kTypes) labels << t.label;

    bool ok = false;
    const QString choice = QInputDialog::getItem(
        this,
        tr("Select File Type"),
        tr("The file type of \"%1\" could not be determined.\n\nPlease select the format:")
            .arg(QString::fromStdString(path.filename().string())),
        labels, 0, false, &ok
#ifdef __APPLE__
        , Qt::WindowFlags{}
#endif
    );

    if (!ok) return {};
    for (const auto& t : kTypes)
        if (t.label == choice) return t.ext;
    return {};
}

void MainWindow::OnLoadDbcRequested()
{
    QFileDialog dialog(this, tr("Load DBC File"));
#ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
#endif
    dialog.setNameFilter(tr("DBC files (*.dbc);;All files (*.*)"));
    dialog.setDirectory(LastDir("dbc",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)));

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString filePath = dialog.selectedFiles().value(0);
    if (filePath.isEmpty())
        return;

    SaveLastDir("dbc", filePath);
    m_currentDbcFilePath = filePath;

    const auto db = parser::dbc::ParseDbcFile(filePath.toStdString());
    if (m_canSignalTree)
        m_canSignalTree->SetDatabase(db);

    util::Logger::Info("[MainWindow] DBC file set: {}", filePath.toStdString());

    // If an ASC file is currently loaded, reload it so DBC signal names appear.
    if (!m_currentLogFilePath.isEmpty() &&
        m_currentLogFilePath.toLower().endsWith(".asc"))
    {
        const std::filesystem::path ascPath(m_currentLogFilePath.toStdString());
        auto parser = CreateParserFor(ascPath);
        if (parser)
        {
            UpdateStatusText(tr("Reloading ASC with DBC signal names…").toStdString());
            m_presenter->LoadLogFile(std::move(parser), ascPath);
        }
    }

    UpdateStatusText(tr("DBC loaded: %1 (%2 messages)")
        .arg(QFileInfo(filePath).fileName())
        .arg(db.messages.size()).toStdString());
}

void MainWindow::OnLoadEvlogTemplatesRequested()
{
    const QString dir = QFileDialog::getExistingDirectory(
        this,
        tr("Select Evlog Template Directory"),
        LastDir("evlog_templates",
            QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
#ifdef __APPLE__
        , QFileDialog::DontUseNativeDialog
#endif
    );

    if (dir.isEmpty())
        return;

    SaveLastDir("evlog_templates", dir + "/dummy"); // SaveLastDir expects a file path
    m_evlogTemplateDir = dir;

    // Count templates loaded as a quick sanity check.
    parser::EvlogTemplateRegistry reg;
    auto tmplResult = reg.LoadFromDirectory(dir.toStdString());
    if (tmplResult.isErr())
        util::Logger::Warn("[MainWindow] LoadFromDirectory failed: {}", tmplResult.error().what());

    UpdateStatusText(tr("Evlog templates loaded: %1 template(s) from %2 — reload .evl file to apply")
        .arg(reg.Count())
        .arg(QFileInfo(dir).fileName())
        .toStdString());
    util::Logger::Info("[MainWindow] Evlog template dir set: {}", dir.toStdString());
}

void MainWindow::OnSaveSession()
{
    QFileDialog dialog(this, tr("Save Session"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setDefaultSuffix("json");
    dialog.setNameFilter(tr("LogViewer Session (*.json);;All files (*.*)"));
    dialog.setDirectory(LastDir("session",
        QString::fromStdString(config::GetConfig().GetDefaultAppPath().string())));
#ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
#endif
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString path = dialog.selectedFiles().value(0);
    if (path.isEmpty())
        return;

    SaveLastDir("session", path);

    try
    {
        nlohmann::json session;
        session["version"]   = 1;
        session["log_file"]  = m_currentLogFilePath.toStdString();
        session["bookmarks"] = m_bookmarksPanel ? m_bookmarksPanel->GetSessionData()
                                                : nlohmann::json::array();
        session["scenarios"] = m_scenariosPanel ? m_scenariosPanel->GetSessionData()
                                                : nlohmann::json::array();

        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text))
            throw std::runtime_error("Cannot open file for writing");
        const std::string json = session.dump(2);
        f.write(json.data(), static_cast<qint64>(json.size()));
        UpdateStatusText(tr("Session saved to %1").arg(path).toStdString());
    }
    catch (const std::exception& ex)
    {
        ShowError(tr("Save Session"), tr("Failed to save session: %1").arg(ex.what()));
    }
}

void MainWindow::OnOpenSession()
{
    QFileDialog dialog(this, tr("Open Session"));
    dialog.setNameFilter(tr("LogViewer Session (*.json);;All files (*.*)"));
    dialog.setDirectory(LastDir("session",
        QString::fromStdString(config::GetConfig().GetDefaultAppPath().string())));
#ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
#endif
    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString path = dialog.selectedFiles().value(0);
    if (path.isEmpty())
        return;

    SaveLastDir("session", path);

    try
    {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
            throw std::runtime_error("Cannot open session file");
        const QByteArray data = f.readAll();
        const nlohmann::json session = nlohmann::json::parse(
            data.constData(), data.constData() + data.size());

        // Load the log file if present
        const std::string logFile = session.value("log_file", std::string{});
        if (!logFile.empty() && std::filesystem::exists(logFile))
        {
            // Clear current data first, then load
            if (m_searchResults) m_searchResults->Clear();
            if (m_searchEdit)    m_searchEdit->clear();
            if (m_events)        m_events->Clear();

            const std::filesystem::path fp(logFile);
            auto parser = CreateParserFor(fp);
            if (!parser) return; // user cancelled type selection
            UpdateStatusText(tr("Loading %1 …").arg(QString::fromStdString(logFile)).toStdString());
            m_presenter->LoadLogFile(std::move(parser), fp);
            m_presenter->SetItemDetailsVisible(true);
            m_currentLogFilePath = QString::fromStdString(logFile);
            AddToRecentFiles(m_currentLogFilePath);
        }
        else if (!logFile.empty())
        {
            QMessageBox::warning(this, tr("Open Session"),
                tr("Log file not found:\n%1\n\nBookmarks and scenarios will be restored without log data.")
                    .arg(QString::fromStdString(logFile)));
        }

        // Restore panels
        if (m_bookmarksPanel && session.contains("bookmarks"))
            m_bookmarksPanel->LoadSessionData(session["bookmarks"]);
        if (m_scenariosPanel && session.contains("scenarios"))
            m_scenariosPanel->LoadSessionData(session["scenarios"]);

        UpdateStatusText(tr("Session loaded from %1").arg(path).toStdString());
    }
    catch (const std::exception& ex)
    {
        ShowError(tr("Open Session"), tr("Failed to load session: %1").arg(ex.what()));
    }
}

void MainWindow::OnToggleTailRequested()
{
    if (!m_tailAction) return;

    if (m_tailAction->isChecked())
    {
        if (m_currentLogFilePath.isEmpty())
        {
            m_tailAction->setChecked(false);
            return;
        }
        if (!m_tailer)
        {
            m_tailer = new FileTailer(this);
            connect(m_tailer, &FileTailer::NewEventsAvailable,
                    this, &MainWindow::OnTailNewEvents);
            connect(m_tailer, &FileTailer::TailingError,
                    this, &MainWindow::OnTailError);
        }
        const std::filesystem::path fp(m_currentLogFilePath.toStdString());
        auto parser = CreateParserFor(fp);
        if (!parser)
        {
            m_tailAction->setChecked(false);
            return;
        }
        m_tailer->Start(fp, std::move(parser), *m_events);
        if (!m_tailer->IsActive())
        {
            // Start() emitted TailingError for unsupported formats
            m_tailAction->setChecked(false);
            return;
        }
        UpdateStatusText("Following file…");
    }
    else
    {
        if (m_tailer) m_tailer->Stop();
        UpdateStatusText("Tailing stopped");
    }
}

void MainWindow::OnTailNewEvents(std::size_t count)
{
    if (m_eventsView)
        m_eventsView->RefreshView();
    MarkAnalysisPanelsDirty();
    UpdateStatusText(
        tr("Following — %1 event(s) total").arg(
            m_events ? static_cast<qulonglong>(m_events->Size()) : 0).toStdString());
    util::Logger::Debug("[MainWindow] Tail: {} new event(s), total={}", count,
        m_events ? m_events->Size() : 0);
}

void MainWindow::OnTailError(const QString& message)
{
    if (m_tailAction)
    {
        m_tailAction->setChecked(false);
        m_tailAction->setEnabled(false);
    }
    if (m_tailer) m_tailer->Stop();
    UpdateStatusText("Tailing error: " + message.toStdString());
    QMessageBox::warning(this, tr("Follow File"), message);
}

void MainWindow::OnClearDataRequested()
{
    try
    {
    util::Logger::Info("[MainWindow] OnClearDataRequested");
        if (m_tailer) { m_tailer->Stop(); }
        if (m_tailAction) { m_tailAction->setChecked(false); m_tailAction->setEnabled(false); }
        m_currentLogFilePath.clear();
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

void MainWindow::ActivateSideBySide()
{
    if (m_contentTabs)
        m_contentTabs->setCurrentWidget(m_eventsStack);
    if (m_eventsStack)
        m_eventsStack->setCurrentIndex(1);
}

void MainWindow::RunFilter(std::function<std::vector<unsigned long>()> worker,
                           const QString& statusMsg)
{
    if (m_filteringInProgress)
        return;
    m_filteringInProgress = true;
    UpdateStatusText(statusMsg.toStdString());
    ToggleProgressVisibility(true);
    ConfigureProgressRange(0); // indeterminate busy animation
    QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);

    std::vector<unsigned long> filteredIndices;
    try {
        filteredIndices = worker();
    } catch (const std::exception& ex) {
        util::Logger::Error("[MainWindow] Error in filter worker: {}", ex.what());
        ToggleProgressVisibility(false);
        m_filteringInProgress = false;
        return;
    }

    m_filteringInProgress = false;
    util::Logger::Debug("[MainWindow] Filter: {} events, {} matches",
        m_events ? m_events->Size() : 0, filteredIndices.size());
    m_eventsView->SetFilteredEvents(filteredIndices);
    m_eventsView->RefreshView();
    ToggleProgressVisibility(false);
    ConfigureProgressRange(100);
    const auto total   = m_events ? m_events->Size() : 0;
    const auto matched = filteredIndices.size();
    UpdateStatusText(
        QString("Filters applied: %1 of %2 events").arg(matched).arg(total).toStdString());
}

void MainWindow::ApplyExtendedFilters()
{
    if (!m_eventsView || !m_events || m_events->Size() == 0)
        return;

    // Apply type filters first, then extended filters on top (AND logic)
    db::EventsContainer* events = m_events;
    RunFilter(
        [this, events]() -> std::vector<unsigned long> {
            // Step 1: Apply type filters
            std::vector<unsigned long> typeFiltered;
            if (m_typeFilterView)
            {
                auto& config = config::GetConfig();
                const auto checkedTypes = m_typeFilterView->CheckedTypes();
                const std::set<std::string> selectedTypeStrings(
                    checkedTypes.begin(), checkedTypes.end());

                typeFiltered.reserve(events->Size());
                for (std::size_t i = 0; i < events->Size(); ++i)
                {
                    const auto& event = events->GetEvent(i);
                    const std::string eventType = event.findByKey(config.typeFilterField);
                    const bool typeMatch = selectedTypeStrings.empty() ||
                        selectedTypeStrings.count(eventType) > 0;

                    if (typeMatch)
                        typeFiltered.push_back(i);
                }
            }
            else
            {
                // No type filter, start with all events
                typeFiltered.resize(events->Size());
                std::iota(typeFiltered.begin(), typeFiltered.end(), 0UL);
            }

            // Step 2: Apply extended filters on top of type-filtered results (AND logic)
            if (typeFiltered.empty())
                return typeFiltered;

            return filters::FilterManager::getInstance().applyFiltersToIndices(typeFiltered, *events);
        },
        tr("Applying filters..."));
}

void MainWindow::ApplyActorFilter()
{
    if (!m_eventsView || !m_events || !m_actorDefPanel)
        return;
    if (m_filteringInProgress)
        return;

    const auto& defs = m_actorDefPanel->Definitions();
    const bool hasEnabled = std::any_of(defs.begin(), defs.end(),
                                        [](const ActorDefinition& d) { return d.enabled; });
    if (!hasEnabled)
    {
        UpdateStatusText("No enabled actor definitions — filter not applied");
        return;
    }

    // Build compiled definition list (matching ActorsPanel::RefreshWithDefinitions logic)
    struct CompiledDef {
        std::string        field;
        QRegularExpression re;
        bool               useCaptures;
    };
    std::vector<CompiledDef> compiled;
    for (const auto& def : defs)
    {
        if (!def.enabled || def.pattern.empty()) continue;
        QRegularExpression re(QString::fromStdString(def.pattern));
        if (!re.isValid()) continue;
        compiled.push_back({def.field, std::move(re), def.useCaptures});
    }

    if (compiled.empty())
    {
        UpdateStatusText("No valid actor patterns — filter not applied");
        return;
    }

    db::EventsContainer* events = m_events;
    RunFilter(
        [events, compiledCopy = std::move(compiled)]()
                          -> std::vector<unsigned long> {
            const size_t total = events->Size();
            std::vector<unsigned long> matched;
            matched.reserve(total);
            for (size_t i = 0; i < total; ++i)
            {
                const db::LogEvent& ev = events->GetEvent(i);
                for (const auto& cdef : compiledCopy)
                {
                    auto testField = [&](const std::string& val) -> bool {
                        const QRegularExpressionMatch m =
                            cdef.re.match(QString::fromStdString(val));
                        if (!m.hasMatch()) return false;
                        if (cdef.useCaptures)
                        {
                            for (int g = 1; g <= cdef.re.captureCount(); ++g)
                                if (!m.captured(g).isEmpty()) return true;
                            return false;
                        }
                        return true;
                    };

                    bool hit = false;
                    if (cdef.field.empty())
                    {
                        for (const auto& [key, val] : ev.getEventItems())
                            if (testField(val)) { hit = true; break; }
                    }
                    else
                    {
                        hit = testField(ev.findByKey(cdef.field));
                    }

                    if (hit) { matched.push_back(static_cast<unsigned long>(i)); break; }
                }
            }
            return matched;
        },
        tr("Applying actor filter…"));
}

void MainWindow::ShowError(const QString& title, const QString& message)
{
    QMessageBox::critical(this, title, message);
}

QString MainWindow::LastDir(const QString& key, const QString& fallback)
{
    QSettings s("LogViewer", "LogViewer");
    return s.value("lastDir/" + key, fallback).toString();
}

void MainWindow::SaveLastDir(const QString& key, const QString& filePath)
{
    QSettings s("LogViewer", "LogViewer");
    s.setValue("lastDir/" + key, QFileInfo(filePath).absolutePath());
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

#ifdef __APPLE__
    // On macOS, also discover plugins bundled inside the .app (Contents/PlugIns/).
    // This makes distributed DMGs work without requiring the user to install plugins manually.
    {
        const std::filesystem::path appDir =
            QCoreApplication::applicationDirPath().toStdString(); // Contents/MacOS
        const std::filesystem::path bundlePlugIns = appDir / ".." / "PlugIns";
        std::error_code ec;
        if (std::filesystem::exists(bundlePlugIns, ec))
        {
            util::Logger::Info("[MainWindow] Scanning bundle PlugIns: {}", bundlePlugIns.string());
            for (const auto& entry : std::filesystem::directory_iterator(bundlePlugIns, ec))
            {
                if (!entry.is_directory()) continue;
                if (!std::filesystem::exists(entry.path() / "config.json")) continue;
                // Avoid duplicates (user may have installed same plugin in app-data dir)
                const auto canonical = std::filesystem::weakly_canonical(entry.path(), ec);
                bool already = false;
                for (const auto& p : discoveredPlugins)
                    if (std::filesystem::weakly_canonical(p, ec) == canonical) { already = true; break; }
                if (!already)
                {
                    discoveredPlugins.push_back(entry.path());
                    util::Logger::Debug("[MainWindow] Found bundle plugin: {}", entry.path().string());
                }
            }
        }
    }
#endif

#ifdef _WIN32
    // On Windows, also scan the installer's plugins directory (e.g. Program Files\LogViewer\plugins\).
    // Plugin ZIPs found there are only READ (discovery); extraction always goes to %APPDATA%,
    // so no write access to Program Files is ever needed.
    {
        const std::filesystem::path installedPlugins =
            config::GetInstalledPluginsDir();
        std::error_code ec;
        if (!installedPlugins.empty() && std::filesystem::exists(installedPlugins, ec))
        {
            util::Logger::Info("[MainWindow] Scanning installed plugins dir: {}",
                               installedPlugins.string());
            for (const auto& entry :
                 std::filesystem::directory_iterator(installedPlugins, ec))
            {
                if (entry.path().extension() != ".zip") continue;
                // Skip if already discovered (user copied the same ZIP to %APPDATA%\plugins)
                const auto canonical =
                    std::filesystem::weakly_canonical(entry.path(), ec);
                bool already = false;
                for (const auto& p : discoveredPlugins)
                    if (std::filesystem::weakly_canonical(p, ec) == canonical)
                    { already = true; break; }
                if (!already)
                {
                    discoveredPlugins.push_back(entry.path());
                    util::Logger::Info("[MainWindow] Found installed plugin ZIP: {}",
                                       entry.path().string());
                }
            }
        }
    }
#endif

    util::Logger::Info("[MainWindow] Discovered {} plugins", discoveredPlugins.size());
    if (m_splash)
        m_splash->Step(tr("Loading %1 plugin(s)…").arg(discoveredPlugins.size()));

    for (const auto& pluginPath : discoveredPlugins) {
        auto loadResult = pluginManager.LoadPlugin(pluginPath);
        if (loadResult.isErr()) {
            const QString name =
                QString::fromStdString(pluginPath.stem().string());
            util::Logger::Error("[MainWindow] Failed to load plugin: {}",
                pluginPath.string());
            if (m_splash)
                m_splash->Warn(tr("Plugin '%1' failed to load").arg(name));
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
    UpdateStatusText("Theme: Dark");
    util::Logger::Info("[MainWindow] Dark theme applied");
}

void MainWindow::OnSetLightTheme()
{
    ApplyTheme(*qApp, 1);
    UpdateStatusText("Theme: Light");
    util::Logger::Info("[MainWindow] Light theme applied");
}

void MainWindow::OnSetSystemTheme()
{
    ApplyTheme(*qApp, 2);
    UpdateStatusText("Theme: System");
    util::Logger::Info("[MainWindow] System theme applied");
}

std::vector<int> MainWindow::GetRowsToExport() const
{
    auto* m = m_eventsView ? m_eventsView->model() : nullptr;
    if (!m) return {};

    // Always export all visible (filtered) rows regardless of selection.
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
    UpdateStatusText("Exporting to CSV...");
    QFileDialog dialog(this, tr("Export to CSV"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilter(tr("CSV files (*.csv);;All files (*.*)"));
    dialog.setDefaultSuffix(QStringLiteral("csv"));
    dialog.setDirectory(LastDir("export",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)));
#ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
#endif
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString path = dialog.selectedFiles().value(0);
    if (path.isEmpty())
        return;
    SaveLastDir("export", path);

    if (!ExportManager::ToCsv(*m_eventsView->model(), rows, path)) {
        UpdateStatusText("Export failed.");
        QMessageBox::warning(this, tr("Export Failed"),
                             tr("Could not write to:\n%1").arg(path));
    } else {
        UpdateStatusText(QString("Exported CSV: %1").arg(path).toStdString());
    }
}

void MainWindow::OnExportJsonRequested()
{
    const auto rows = GetRowsToExport();
    if (rows.empty()) {
        QMessageBox::information(this, tr("Export"), tr("No data to export."));
        return;
    }
    UpdateStatusText("Exporting to JSON...");
    QFileDialog dialog(this, tr("Export to JSON"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilter(tr("JSON files (*.json);;All files (*.*)"));
    dialog.setDefaultSuffix(QStringLiteral("json"));
    dialog.setDirectory(LastDir("export",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)));
#ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
#endif
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString path = dialog.selectedFiles().value(0);
    if (path.isEmpty())
        return;
    SaveLastDir("export", path);

    if (!ExportManager::ToJson(*m_eventsView->model(), rows, path)) {
        UpdateStatusText("Export failed.");
        QMessageBox::warning(this, tr("Export Failed"),
                             tr("Could not write to:\n%1").arg(path));
    } else {
        UpdateStatusText(QString("Exported JSON: %1").arg(path).toStdString());
    }
}

void MainWindow::OnExportXmlRequested()
{
    const auto rows = GetRowsToExport();
    if (rows.empty()) {
        QMessageBox::information(this, tr("Export"), tr("No data to export."));
        return;
    }
    UpdateStatusText("Exporting to XML...");
    QFileDialog dialog(this, tr("Export to XML"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilter(tr("XML files (*.xml);;All files (*.*)"));
    dialog.setDefaultSuffix(QStringLiteral("xml"));
    dialog.setDirectory(LastDir("export",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)));
#ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
#endif
    if (dialog.exec() != QDialog::Accepted)
        return;
    const QString path = dialog.selectedFiles().value(0);
    if (path.isEmpty())
        return;
    SaveLastDir("export", path);

    if (!ExportManager::ToXml(*m_eventsView->model(), rows, path)) {
        UpdateStatusText("Export failed.");
        QMessageBox::warning(this, tr("Export Failed"),
                             tr("Could not write to:\n%1").arg(path));
    } else {
        UpdateStatusText(QString("Exported XML: %1").arg(path).toStdString());
    }
}

void MainWindow::OnGenerateReportFromDashboard()
{
    if (!m_events || !m_eventsView)
        return;

    const auto rows = GetRowsToExport();
    if (rows.empty()) {
        QMessageBox::information(this, tr("Generate Report"), tr("No data to generate report."));
        return;
    }

    UpdateStatusText("Generating report...");

    // Create report generator
    ui::qt::utils::ReportGenerator::ReportOptions options;
    options.title = "Log Analysis Report";
    options.format = ui::qt::utils::ReportGenerator::ReportFormat::HTML;
    options.includeSummary = true;
    options.includeStatistics = true;
    options.includeTimeline = true;
    options.includeTrends = true;
    options.includeEventList = true;
    options.includeActorAnalysis = true;
    options.maxEventsInReport = static_cast<int>(rows.size());

    ui::qt::utils::ReportGenerator generator(*m_events);
    QString reportContent = generator.generateReport(rows, options);

    if (reportContent.isEmpty()) {
        UpdateStatusText("Report generation failed.");
        QMessageBox::warning(this, tr("Report Generation Failed"),
                             tr("Failed to generate report from events."));
        return;
    }

    // Save report to file
    QFileDialog dialog(this, tr("Save Report"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setNameFilter(tr("HTML files (*.html);;All files (*.*)"));
    dialog.setDefaultSuffix(QStringLiteral("html"));
    dialog.setDirectory(LastDir("reports",
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation)));
#ifdef __APPLE__
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
#endif

    if (dialog.exec() != QDialog::Accepted)
        return;

    const QString path = dialog.selectedFiles().value(0);
    if (path.isEmpty())
        return;

    SaveLastDir("reports", path);

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        UpdateStatusText("Failed to save report.");
        QMessageBox::warning(this, tr("Report Save Failed"),
                             tr("Could not write to:\n%1").arg(path));
        return;
    }

    QTextStream stream(&file);
    stream << reportContent;
    file.close();

    UpdateStatusText(QString("Report generated: %1").arg(path).toStdString());

    // Ask user if they want to open the report
    if (QMessageBox::question(this, tr("Report Generated"),
            tr("Report saved to:\n%1\n\nOpen in browser?").arg(path))
            == QMessageBox::Yes) {
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
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

    // 4. Keep the zip as a local download cache — do NOT delete it immediately
    // after loading. Deleting a downloaded binary moments after executing it
    // ("write to disk -> load -> remove") is the exact dropper cleanup sequence
    // Windows Defender's Wacatac heuristic is trained on. The file lives in
    // AppLocalDataLocation/plugin_downloads/ and will be overwritten on the
    // next update, which is a normal cache-management pattern AV vendors allow.

    util::Logger::Info("[MainWindow] Plugin {} updated successfully", id);
    UpdateStatusText(tr("Plugin %1 updated").arg(pluginId).toStdString());
}

void MainWindow::OnProfileSaveRequested(const QString& name)
{
    if (!m_profilesPanel) return;

    FilterProfile fp;
    fp.name = name.toStdString();

    if (m_timeRangePanel)
        fp.timeRange = m_timeRangePanel->GetState();

    if (m_actorsPanel)
    {
        const auto keys = m_actorsPanel->GetUncheckedActors();
        fp.uncheckedActors.assign(keys.begin(), keys.end());
    }

    if (m_typeFilterView && !m_typeFilterView->Empty())
    {
        fp.hasTypeFilter = true;
        fp.checkedTypes  = m_typeFilterView->CheckedTypes();
    }

    m_profilesPanel->StoreProfile(fp);
}

void MainWindow::OnProfileLoadRequested(const FilterProfile& profile)
{
    // Apply time range filter
    if (m_timeRangePanel)
    {
        m_timeRangePanel->SetState(profile.timeRange);
        if (profile.timeRange.active)
            m_timeRangePanel->Apply();
        else
            m_timeRangePanel->Clear();
    }

    // Restore actor check state
    if (m_actorsPanel)
    {
        std::set<std::string> unchecked(
            profile.uncheckedActors.begin(), profile.uncheckedActors.end());
        m_actorsPanel->RestoreUncheckedActors(unchecked);
    }

    // Restore type filter (only when the profile captured one)
    if (profile.hasTypeFilter && m_typeFilterView)
    {
        m_typeFilterView->SetCheckedTypes(profile.checkedTypes);
        OnApplyFilterClicked();
    }
}

void MainWindow::MarkAnalysisPanelsDirty()
{
    if (m_dashboardPanel) m_dirtyPanels.insert(m_dashboardPanel);
    if (m_statsPanel)   m_dirtyPanels.insert(m_statsPanel);
    if (m_patternPanel) m_dirtyPanels.insert(m_patternPanel);
    if (m_actorsPanel)  m_dirtyPanels.insert(m_actorsPanel);
    if (m_signalPlotPanel) m_dirtyPanels.insert(m_signalPlotPanel);
    if (m_timelinePanel) m_dirtyPanels.insert(m_timelinePanel);
    if (m_tracePanel)    m_dirtyPanels.insert(m_tracePanel);
    if (m_sequencePanel) m_dirtyPanels.insert(m_sequencePanel);

    // Restart the timer — rapid filter changes collapse into one refresh.
    if (m_panelRefreshTimer)
        m_panelRefreshTimer->start();
}

void MainWindow::RefreshCurrentAnalysisPanel()
{
    if (!m_contentTabs) return;
    QWidget* current = m_contentTabs->currentWidget();
    if (!m_dirtyPanels.count(current)) return;

    m_dirtyPanels.erase(current);

    if (current == m_dashboardPanel)
        m_dashboardPanel->Refresh();
    else if (current == m_statsPanel)
        m_statsPanel->Refresh();
    else if (current == m_patternPanel)
        m_patternPanel->Refresh();
    else if (current == m_actorsPanel)
        m_actorsPanel->Refresh();
    else if (current == m_signalPlotPanel)
        m_signalPlotPanel->Refresh();
    else if (current == m_timelinePanel)
        m_timelinePanel->Refresh();
    else if (current == m_tracePanel)
        m_tracePanel->Refresh();
    else if (current == m_sequencePanel)
        m_sequencePanel->Refresh();
}

} // namespace ui::qt
