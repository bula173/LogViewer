#include "MainWindow.hpp"

#include "EventsContainer.hpp"
#include "FilterManager.hpp"
#include "IControler.hpp"
#include "MainWindowPresenter.hpp"
#include "EventsTableView.hpp"
#include "FiltersPanel.hpp"
#include "ItemDetailsView.hpp"
#include "SearchResultsView.hpp"
#include "TypeFilterView.hpp"
#include "OllamaSetupDialog.hpp"
#include "LogFileLoadDialog.hpp"
#include "Logger.hpp"
#include "Config.hpp"
#include "ConfigEditorDialog.hpp"
#include "StructuredConfigDialog.hpp"
#include "version.h"
#include "PluginManager.hpp"
#include "IAnalysisPlugin.hpp"
#include "IAIPlugin.hpp"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QFileDialog>
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
#include <QKeySequence>
#include <QDockWidget>

#include <nlohmann/json.hpp>

#include <filesystem>
#include <stdexcept>

namespace ui::qt
{

MainWindow::MainWindow(mvc::IController& controller,
    db::EventsContainer& events, QWidget* parent)
    : QMainWindow(parent)
{
    m_events = &events;
    util::Logger::Info("[MainWindow] Initializing main window");

    try {
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
        QSettings settings("LogViewer", "LogViewerQt");
        const QByteArray geom = settings.value("windowGeometry").toByteArray();
        if (!geom.isEmpty()) {
            restoreGeometry(geom);
        }
        const QByteArray state = settings.value("windowState").toByteArray();
        if (!state.isEmpty()) {
            restoreState(state);
        }
        
        // Force plugin config dock to be properly positioned after state restoration
        if (m_pluginConfigDock) {
            m_pluginConfigDock->setFloating(false);
            // Ensure it's in the left dock area
            if (!dockWidgetArea(m_pluginConfigDock)) {
                addDockWidget(Qt::LeftDockWidgetArea, m_pluginConfigDock);
            }
            // Tab with filters
            tabifyDockWidget(m_filtersDock, m_pluginConfigDock);
        }

    } catch (const std::exception& ex) {
        util::Logger::Error("[MainWindow] Initialization failed: {}", ex.what());
        throw; // Re-throw to let the application handle it
    }
}

MainWindow::~MainWindow()
{
    // Clean up presenter first to ensure proper disconnection before Qt widgets are destroyed
    m_presenter.reset();

    // Save window layout
    QSettings settings("LogViewer", "LogViewerQt");
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
        m_aiService = nullptr;

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

    m_filtersDock->setWidget(m_filterTabs);
    addDockWidget(Qt::LeftDockWidgetArea, m_filtersDock);

    // Generic Plugin Configuration dock with tabs for multiple plugins
    m_pluginConfigDock = new QDockWidget("Plugin Configuration", this);
    if (!m_pluginConfigDock) {
        throw std::runtime_error("Failed to create plugin config dock");
    }
    m_pluginConfigDock->setObjectName("PluginConfigurationDockWidget");
    m_pluginConfigDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_pluginConfigDock->setFeatures(QDockWidget::DockWidgetMovable |
                                    QDockWidget::DockWidgetFloatable |
                                    QDockWidget::DockWidgetClosable);
    
    // Create tab widget to hold multiple plugin configurations
    m_pluginConfigTabs = new QTabWidget(m_pluginConfigDock);
    m_pluginConfigTabs->setObjectName("PluginConfigTabs");
    m_pluginConfigTabs->setTabsClosable(false);
    
    // Set size policy to allow the tab widget to shrink/expand with available space
    m_pluginConfigTabs->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
    
    m_pluginConfigDock->setWidget(m_pluginConfigTabs);
    
    addDockWidget(Qt::LeftDockWidgetArea, m_pluginConfigDock);
    m_pluginConfigDock->setFloating(false);  // Ensure it's docked, not floating
    tabifyDockWidget(m_filtersDock, m_pluginConfigDock);  // Tab with filters in left panel
    m_pluginConfigDock->hide(); // Hidden until plugins provide configuration UI

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

    auto* aiSetupAction = toolsMenu->addAction(tr("AI Analysis &Setup..."));
    connect(aiSetupAction, &QAction::triggered, this, [this]() {
        ui::qt::OllamaSetupDialog dlg(this);
        dlg.exec();
    });
    
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
    viewMenu->addAction(m_pluginConfigDock->toggleViewAction());
    viewMenu->addAction(m_detailsDock->toggleViewAction());
    viewMenu->addAction(m_bottomDock->toggleViewAction());
    
    viewMenu->addSeparator();
    
    auto* resetLayoutAction = viewMenu->addAction(tr("&Reset Layout"));
    connect(resetLayoutAction, &QAction::triggered, this, [this]() {
        // Reset all docks to default positions
        if (m_filtersDock) {
            m_filtersDock->setFloating(false);
            addDockWidget(Qt::LeftDockWidgetArea, m_filtersDock);
            m_filtersDock->show();
        }
        if (m_pluginConfigDock) {
            m_pluginConfigDock->setFloating(false);
            addDockWidget(Qt::LeftDockWidgetArea, m_pluginConfigDock);
            // Only show if there are config tabs
            if (m_pluginConfigTabs && m_pluginConfigTabs->count() > 0) {
                m_pluginConfigDock->show();
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

    util::Logger::Debug("[MainWindow] Presenter initialized successfully");
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
            m_eventsView->RefreshView();
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
    RefreshAIProvider();
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
    
    // Set plugins directory relative to application
    std::filesystem::path pluginsDir = config::GetConfig().GetDefaultAppPath() / "plugins";
    
    pluginManager.SetPluginsDirectory(pluginsDir);
    util::Logger::Info("[MainWindow] Plugin directory set to: {}", pluginsDir.string());
}

void MainWindow::loadPlugins() {
    auto& pluginManager = plugin::PluginManager::GetInstance();
    const std::string configuredAiPlugin = config::GetConfig().aiPluginId;
    
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
        if (it != loadedPlugins.end() && (it->second.autoLoad || pluginId == configuredAiPlugin)) {
            util::Logger::Info("[MainWindow] Enabling plugin: {} (autoLoad={} configuredAi={})",
                pluginId, it->second.autoLoad, pluginId == configuredAiPlugin);
            auto enableResult = pluginManager.EnablePlugin(pluginId);
            if (enableResult.isErr()) {
                util::Logger::Error("[MainWindow] Failed to enable plugin {}: {}",
                    pluginId, enableResult.error().what());
            }
        }
    }

    RefreshAIProvider();
}

void MainWindow::RemoveAIPluginPanel()
{
    if (!m_bottomTabs || !m_aiPluginPanel)
        return;

    const int idx = m_bottomTabs->indexOf(m_aiPluginPanel);
    if (idx >= 0)
    {
        m_bottomTabs->removeTab(idx);
    }
    m_aiPluginPanel->deleteLater();
    m_aiPluginPanel = nullptr;
    m_activeAiPluginId.clear();
}

void MainWindow::RemoveAITab()
{
    if (!m_contentTabs || !m_aiTabWidget)
        return;

    const int idx = m_contentTabs->indexOf(m_aiTabWidget);
    if (idx >= 0)
    {
        m_contentTabs->removeTab(idx);
    }
    m_aiTabWidget->deleteLater();
    m_aiTabWidget = nullptr;
    m_aiTabIndex = -1;
}

void MainWindow::RemoveAIConfigWidget()
{
    // Config tab removal is handled by generic removePluginConfigTab
    // This function is kept for compatibility but does nothing now
    util::Logger::Debug("[MainWindow] RemoveAIConfigWidget - config handled by generic plugin system");
}

void MainWindow::RemoveAIChatWidget()
{
    if (!m_bottomTabs || !m_aiChatWidget)
        return;

    const int idx = m_bottomTabs->indexOf(m_aiChatWidget);
    if (idx >= 0)
    {
        m_bottomTabs->removeTab(idx);
    }
    m_aiChatWidget->deleteLater();
    m_aiChatWidget = nullptr;
}

void MainWindow::UseBuiltInAIProvider()
{
    // No built-in AI UI; fallback disables AI integration
    RemoveAITab();
    RemoveAIConfigWidget();
    RemoveAIChatWidget();
    RemoveAIPluginPanel();
    m_aiService.reset();
    m_activeAiPluginId.clear();
    util::Logger::Info("[MainWindow] No AI plugin active; AI features disabled");
}

void MainWindow::UseAIPluginProvider(const std::string& pluginId, plugin::IAIPlugin* aiPlugin)
{
    if (!aiPlugin || !m_events)
    {
        util::Logger::Warn("[MainWindow] UseAIPluginProvider called with missing plugin or events");
        UseBuiltInAIProvider();
        return;
    }

    // Plugin will load its own config file; pass empty settings to CreateService
    nlohmann::json settings = nlohmann::json::object();
    util::Logger::Debug("[MainWindow] Initializing AI plugin {} with plugin-managed config", pluginId);
    
    auto pluginService = aiPlugin->CreateService(settings);
    if (!pluginService)
    {
        util::Logger::Error("[MainWindow] AI plugin {} failed to create service", pluginId);
        UseBuiltInAIProvider();
        return;
    }

    m_aiService = pluginService;
    m_activeAiPluginId = pluginId;

    // Provide events container to the AI plugin
    aiPlugin->SetEventsContainer(std::shared_ptr<db::EventsContainer>(m_events, [](db::EventsContainer*){}));
    aiPlugin->OnEventsUpdated();

    // Remove built-in UI and plugin remnants
    RemoveAITab();
    RemoveAIConfigWidget();
    RemoveAIChatWidget();
    RemoveAIPluginPanel();

    if (auto* plugin = plugin::PluginManager::GetInstance().GetPlugin(pluginId))
    {
        util::Logger::Debug("[MainWindow] Creating UI widgets for AI plugin: {}", pluginId);
        
        // Ensure config tab is created for AI plugin
        // Check if it already exists, if not create it
        if (m_pluginConfigTabIndices.find(pluginId) == m_pluginConfigTabIndices.end()) {
            createPluginConfigTab(pluginId, plugin);
        }
        
        // Create analysis tab
        if (m_contentTabs)
        {
            m_aiTabWidget = plugin->CreateTab(m_contentTabs);
            util::Logger::Debug("[MainWindow] Got analysis tab widget: {}", m_aiTabWidget ? "yes" : "no");
            if (m_aiTabWidget)
            {
                const std::string tabName = plugin->GetMetadata().name.empty() ? pluginId : plugin->GetMetadata().name;
                m_aiTabIndex = m_contentTabs->addTab(m_aiTabWidget, QString::fromStdString(tabName));
                util::Logger::Info("[MainWindow] Added AI analysis tab '{}' at index {}", tabName, m_aiTabIndex);
            }
        }

        // Create bottom panel (chat)
        if (m_bottomTabs)
        {
            m_aiPluginPanel = plugin->CreateBottomPanel(m_bottomTabs, m_aiService.get(), m_events);
            util::Logger::Debug("[MainWindow] Got bottom panel widget: {}", m_aiPluginPanel ? "yes" : "no");
            if (m_aiPluginPanel)
            {
                const std::string tabName = plugin->GetMetadata().name.empty() ? pluginId : plugin->GetMetadata().name + " Chat";
                int chatTabIndex = m_bottomTabs->addTab(m_aiPluginPanel, QString::fromStdString(tabName));
                util::Logger::Info("[MainWindow] Added AI chat tab '{}' at index {}", tabName, chatTabIndex);
            }
        }
    }

    util::Logger::Info("[MainWindow] Activated AI plugin provider: {}", pluginId);
}

void MainWindow::RefreshAIProvider()
{
    auto& cfg = config::GetConfig();
    auto& pluginManager = plugin::PluginManager::GetInstance();

    if (!cfg.aiPluginId.empty())
    {
        const auto& loaded = pluginManager.GetLoadedPlugins();
        auto it = loaded.find(cfg.aiPluginId);
        if (it != loaded.end())
        {
            if (!it->second.enabled)
            {
                auto enableResult = pluginManager.EnablePlugin(cfg.aiPluginId);
                if (enableResult.isErr())
                {
                    util::Logger::Warn("[MainWindow] Failed to enable configured AI plugin {}: {}",
                        cfg.aiPluginId, enableResult.error().what());
                }
            }

            if (auto* plugin = pluginManager.GetPlugin(cfg.aiPluginId);
                plugin && plugin->GetMetadata().type == plugin::PluginType::AIProvider)
            {
                if (auto* aiInterface = plugin->GetAIPluginInterface())
                {
                    UseAIPluginProvider(cfg.aiPluginId, aiInterface);
                    return;
                }
            }
        }
        util::Logger::Warn("[MainWindow] Configured AI plugin {} not available or disabled; falling back",
            cfg.aiPluginId);
    }

    // Only remove AI UI if there's no active service (don't override plugin-enabled AI)
    if (!m_aiService || m_activeAiPluginId.empty())
    {
        UseBuiltInAIProvider();
    }
    else
    {
        util::Logger::Info("[MainWindow] AI provider already active ({}), skipping fallback", m_activeAiPluginId);
    }
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
            if (pluginId == m_activeAiPluginId) {
                RemoveAITab();
                RemoveAIConfigWidget();
                RemoveAIChatWidget();
                RemoveAIPluginPanel();
                UseBuiltInAIProvider();
            }
            break;
            
        case PluginEvent::Enabled:
            util::Logger::Info("[MainWindow] Plugin enabled: {}", pluginId);
            createPluginTab(pluginId, plugin);
            createPluginFilterTab(pluginId, plugin);
            createPluginConfigTab(pluginId, plugin);
            if (plugin && plugin->GetMetadata().type == plugin::PluginType::AIProvider) {
                // Directly activate this AI provider since user enabled it
                if (auto* aiInterface = plugin->GetAIPluginInterface()) {
                    UseAIPluginProvider(pluginId, aiInterface);
                }
            }
            break;
            
        case PluginEvent::Disabled:
            util::Logger::Info("[MainWindow] Plugin disabled: {}", pluginId);
            removePluginTab(pluginId);
            removePluginFilterTab(pluginId);
            removePluginConfigTab(pluginId);
            if (pluginId == m_activeAiPluginId) {
                RemoveAITab();
                RemoveAIConfigWidget();
                RemoveAIChatWidget();
                RemoveAIPluginPanel();
                UseBuiltInAIProvider();
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
    
    // AI provider tab is created during provider activation to ensure service/events are ready
    if (plugin->GetMetadata().type == plugin::PluginType::AIProvider) {
        util::Logger::Info("[MainWindow] Skipping immediate tab creation for AI provider: {}", pluginId);
        return;
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
    
    // If this is an analysis plugin, set up events container
    if (metadata.type == plugin::PluginType::Analyzer) {
        auto* analysisPlugin = plugin->GetAnalysisInterface();
        if (analysisPlugin && m_events) {
            analysisPlugin->SetEventsContainer(
                std::shared_ptr<db::EventsContainer>(m_events, [](db::EventsContainer*){}));
            util::Logger::Info("[MainWindow] Set events container for analysis plugin: {}", 
                metadata.name);
        }
    }
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

void MainWindow::createPluginFilterTab(const std::string& pluginId, plugin::IPlugin* plugin) {
    if (!plugin) {
        util::Logger::Error("[MainWindow] createPluginFilterTab called with null plugin");
        return;
    }
    
    if (!m_filterTabs) {
        util::Logger::Error("[MainWindow] createPluginFilterTab called but m_filterTabs is null");
        return;
    }
    
    if (plugin->GetMetadata().type == plugin::PluginType::AIProvider) {
        util::Logger::Info("[MainWindow] Skipping filter tab creation for AI provider: {}", pluginId);
        return;
    }

    util::Logger::Info("[MainWindow] Creating filter tab for plugin: {}", pluginId);
    
    // Try to create a filter tab from the plugin
    QWidget* filterTab = plugin->CreateFilterTab(this);
    if (!filterTab) {
        util::Logger::Info("[MainWindow] Plugin {} does not provide a filter tab widget", pluginId);
        return;
    }
    
    auto metadata = plugin->GetMetadata();
    QString tabName = QString::fromStdString(metadata.name + " Config");
    int tabIndex = m_filterTabs->addTab(filterTab, tabName);
    m_pluginFilterTabIndices[pluginId] = tabIndex;
    util::Logger::Info("[MainWindow] Created plugin filter tab: {} at index {}", metadata.name, tabIndex);
}

void MainWindow::removePluginFilterTab(const std::string& pluginId) {
    util::Logger::Info("[MainWindow] Removing filter tab for plugin: {}", pluginId);
    
    // Find and remove the filter tab for this plugin
    auto it = m_pluginFilterTabIndices.find(pluginId);
    if (it != m_pluginFilterTabIndices.end()) {
        int tabIndex = it->second;
        
        // Remove the tab
        if (tabIndex >= 0 && tabIndex < m_filterTabs->count()) {
            QWidget* widget = m_filterTabs->widget(tabIndex);
            m_filterTabs->removeTab(tabIndex);
            if (widget) {
                widget->deleteLater();
            }
            util::Logger::Info("[MainWindow] Removed plugin filter tab at index {}", tabIndex);
        }
        
        // Update indices for tabs that came after this one
        m_pluginFilterTabIndices.erase(it);
        for (auto& [id, idx] : m_pluginFilterTabIndices) {
            if (idx > tabIndex) {
                idx--;
            }
        }
    }
}

void MainWindow::createPluginConfigTab(const std::string& pluginId, plugin::IPlugin* plugin) {
    if (!plugin) {
        util::Logger::Error("[MainWindow] createPluginConfigTab called with null plugin");
        return;
    }
    
    if (!m_pluginConfigTabs) {
        util::Logger::Error("[MainWindow] createPluginConfigTab called but m_pluginConfigTabs is null");
        return;
    }
    
    util::Logger::Info("[MainWindow] Creating config tab for plugin: {}", pluginId);
    
    // Try to get configuration UI from the plugin
    QWidget* configWidget = plugin->GetConfigurationUI();
    if (!configWidget) {
        util::Logger::Info("[MainWindow] Plugin {} does not provide a configuration UI", pluginId);
        return;
    }
    
    // Wrap the config widget in a scroll area to prevent overlap with bottom panel
    auto* scrollArea = new QScrollArea(m_pluginConfigTabs);
    scrollArea->setWidget(configWidget);
    scrollArea->setWidgetResizable(true);  // Allow widget to resize with available space
    scrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    scrollArea->setFrameShape(QFrame::NoFrame);  // Remove frame border for cleaner look
    
    auto metadata = plugin->GetMetadata();
    QString tabName = QString::fromStdString(metadata.name.empty() ? pluginId : metadata.name);
    int tabIndex = m_pluginConfigTabs->addTab(scrollArea, tabName);
    m_pluginConfigTabIndices[pluginId] = tabIndex;
    
    // Show the dock when first config tab is added
    if (m_pluginConfigDock && m_pluginConfigTabs->count() == 1) {
        m_pluginConfigDock->setFloating(false);  // Ensure it's not floating
        m_pluginConfigDock->show();
        m_pluginConfigDock->raise();  // Bring to front if tabified
    }
    
    util::Logger::Info("[MainWindow] Created plugin config tab: {} at index {}", tabName.toStdString(), tabIndex);
}

void MainWindow::removePluginConfigTab(const std::string& pluginId) {
    util::Logger::Info("[MainWindow] Removing config tab for plugin: {}", pluginId);
    
    auto it = m_pluginConfigTabIndices.find(pluginId);
    if (it != m_pluginConfigTabIndices.end()) {
        int tabIndex = it->second;
        
        // Remove the tab
        if (tabIndex >= 0 && tabIndex < m_pluginConfigTabs->count()) {
            QWidget* widget = m_pluginConfigTabs->widget(tabIndex);
            m_pluginConfigTabs->removeTab(tabIndex);
            if (widget) {
                widget->deleteLater();
            }
            util::Logger::Info("[MainWindow] Removed plugin config tab at index {}", tabIndex);
        }
        
        // Hide dock if no more config tabs
        if (m_pluginConfigDock && m_pluginConfigTabs->count() == 0) {
            m_pluginConfigDock->hide();
        }
        
        // Update indices for tabs that came after this one
        m_pluginConfigTabIndices.erase(it);
        for (auto& [id, idx] : m_pluginConfigTabIndices) {
            if (idx > tabIndex) {
                idx--;
            }
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
    RemoveAITab();
    RemoveAIConfigWidget();
    RemoveAIChatWidget();
    RemoveAIPluginPanel();

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

} // namespace ui::qt
