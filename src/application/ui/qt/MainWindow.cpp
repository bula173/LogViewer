#include "ui/qt/MainWindow.hpp"

#include "db/EventsContainer.hpp"
#include "filters/FilterManager.hpp"
#include "mvc/IControler.hpp"
#include "ui/MainWindowPresenter.hpp"
#include "ui/qt/EventsTableView.hpp"
#include "ui/qt/FiltersPanel.hpp"
#include "ui/qt/ItemDetailsView.hpp"
#include "ui/qt/SearchResultsView.hpp"
#include "ui/qt/TypeFilterView.hpp"
#include "ui/qt/AIAnalysisPanel.hpp"
#include "ui/qt/AIConfigPanel.hpp"
#include "ui/qt/AIChatPanel.hpp"
#include "ui/qt/OllamaSetupDialog.hpp"
#include "ui/qt/LogFileLoadDialog.hpp"
#include "ai/AIServiceFactory.hpp"
#include "ai/LogAnalyzer.hpp"
#include "application/util/Logger.hpp"
#include "config/Config.hpp"
#include "ui/qt/ConfigEditorDialog.hpp"
#include "ui/qt/StructuredConfigDialog.hpp"
#include "main/version.h"

#include <QAction>
#include <QApplication>
#include <QCoreApplication>
#include <QDesktopServices>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLabel>
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
    InitializeUi(events);
    SetupMenus();
    InitializePresenter(controller, events);
    util::Logger::Info("[MainWindow] Main window initialized");
    
    // Restore window layout
    QSettings settings("LogViewer", "LogViewerQt");
    restoreGeometry(settings.value("windowGeometry").toByteArray());
    restoreState(settings.value("windowState").toByteArray());
}

MainWindow::~MainWindow()
{
    // Save window layout
    QSettings settings("LogViewer", "LogViewerQt");
    settings.setValue("windowGeometry", saveGeometry());
    settings.setValue("windowState", saveState());
}

void MainWindow::InitializeUi(db::EventsContainer& events)
{
    util::Logger::Debug("[MainWindow] InitializeUi: events size={}",
        events.Size());

    // ===== STATUS BAR =====
    m_statusLabel = new QLabel("Ready", this);
    statusBar()->addWidget(m_statusLabel, 1);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(12);
    statusBar()->addPermanentWidget(m_progressBar, 0);

    // ===== CENTRAL WIDGET: Main content area with tabs =====
    auto* contentTabs = new QTabWidget(this);
    
    // Events view tab
    m_eventsView = new EventsTableView(events, contentTabs);
    contentTabs->addTab(m_eventsView, "Events");
    
    // AI Analysis tab - use factory to create appropriate client
    auto& config = config::GetConfig();
    m_aiService = ai::AIServiceFactory::CreateClient(
        config.aiProvider,
        config.GetApiKeyForProvider(config.aiProvider),
        config.ollamaBaseUrl,
        config.ollamaDefaultModel
    );
    m_aiAnalyzer = std::make_shared<ai::LogAnalyzer>(m_aiService, events);
    m_aiPanel = new AIAnalysisPanel(m_aiService, m_aiAnalyzer, m_eventsView, contentTabs);
    contentTabs->addTab(m_aiPanel, "AI Analysis");
    
    // Create AI configuration panel (shared references with AI Analysis)
    m_aiConfigPanel = new AIConfigPanel(m_aiService, m_aiAnalyzer, this);
    m_aiPanel->SetConfigPanel(m_aiConfigPanel);
    
    setCentralWidget(contentTabs);

    // ===== LEFT DOCK: Filters and AI Configuration =====
    // Filters dock
    m_filtersDock = new QDockWidget("Filters", this);
    m_filtersDock->setObjectName("FiltersDockWidget");
    m_filtersDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_filtersDock->setFeatures(QDockWidget::DockWidgetMovable | 
                               QDockWidget::DockWidgetFloatable | 
                               QDockWidget::DockWidgetClosable);
    
    m_filterTabs = new QTabWidget(m_filtersDock);
    
    auto* filtersTab = new QWidget(m_filterTabs);
    auto* filtersLayout = new QVBoxLayout(filtersTab);
    m_filtersPanel = new FiltersPanel(filtersTab);
    filtersLayout->addWidget(m_filtersPanel);
    filtersTab->setLayout(filtersLayout);

    auto* typeTab = new QWidget(m_filterTabs);
    auto* typeLayout = new QVBoxLayout(typeTab);
    typeLayout->addWidget(new QLabel("Type:", typeTab));
    m_typeFilterView = new TypeFilterView(typeTab);
    typeLayout->addWidget(m_typeFilterView);
    m_applyFilterButton = new QPushButton("Apply Filter", typeTab);
    typeLayout->addWidget(m_applyFilterButton);
    typeTab->setLayout(typeLayout);

    m_filterTabs->addTab(filtersTab, "Extended Filters");
    m_filterTabs->addTab(typeTab, "Type Filters");
    
    m_filtersDock->setWidget(m_filterTabs);
    addDockWidget(Qt::LeftDockWidgetArea, m_filtersDock);
    
    // AI Configuration dock
    m_aiConfigDock = new QDockWidget("AI Configuration", this);
    m_aiConfigDock->setObjectName("AIConfigurationDockWidget");
    m_aiConfigDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_aiConfigDock->setFeatures(QDockWidget::DockWidgetMovable | 
                                QDockWidget::DockWidgetFloatable | 
                                QDockWidget::DockWidgetClosable);
    m_aiConfigDock->setWidget(m_aiConfigPanel);
    addDockWidget(Qt::LeftDockWidgetArea, m_aiConfigDock);

    // ===== RIGHT DOCK: Item Details =====
    m_detailsDock = new QDockWidget("Item Details", this);
    m_detailsDock->setObjectName("ItemDetailsDockWidget");
    m_detailsDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    m_detailsDock->setFeatures(QDockWidget::DockWidgetMovable | 
                               QDockWidget::DockWidgetFloatable | 
                               QDockWidget::DockWidgetClosable);
    
    m_itemDetailsView = new ItemDetailsView(events, m_detailsDock);
    m_detailsDock->setWidget(m_itemDetailsView);
    addDockWidget(Qt::RightDockWidgetArea, m_detailsDock);

    // ===== BOTTOM DOCK: Search & AI Chat =====
    m_bottomDock = new QDockWidget("Tools", this);
    m_bottomDock->setObjectName("ToolsDockWidget");
    m_bottomDock->setAllowedAreas(Qt::BottomDockWidgetArea | Qt::TopDockWidgetArea);
    m_bottomDock->setFeatures(QDockWidget::DockWidgetMovable | 
                              QDockWidget::DockWidgetFloatable | 
                              QDockWidget::DockWidgetClosable);
    
    auto* bottomTabs = new QTabWidget(m_bottomDock);
    
    // Search tab
    auto* searchPanel = new QWidget(bottomTabs);
    auto* searchLayout = new QVBoxLayout(searchPanel);
    searchLayout->setContentsMargins(4, 4, 4, 4);
    searchLayout->setSpacing(8);

    auto* searchRow = new QHBoxLayout();
    m_searchEdit = new QLineEdit(searchPanel);
    m_searchEdit->setPlaceholderText("Enter search query");
    m_searchButton = new QPushButton("Search", searchPanel);
    searchRow->addWidget(m_searchEdit);
    searchRow->addWidget(m_searchButton);

    m_searchResults = new SearchResultsView(searchPanel);

    searchLayout->addLayout(searchRow);
    searchLayout->addWidget(m_searchResults, 1);
    searchPanel->setLayout(searchLayout);
    
    bottomTabs->addTab(searchPanel, "Search");
    
    // AI Chat tab
    auto* chatPanel = new AIChatPanel(m_aiService, events, bottomTabs);
    bottomTabs->addTab(chatPanel, "AI Chat");
    
    m_bottomDock->setWidget(bottomTabs);
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

    // View menu for dock widgets
    auto* viewMenu = bar->addMenu(tr("&View"));
    viewMenu->addAction(m_filtersDock->toggleViewAction());
    viewMenu->addAction(m_aiConfigDock->toggleViewAction());
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
        if (m_aiConfigDock) {
            m_aiConfigDock->setFloating(false);
            addDockWidget(Qt::LeftDockWidgetArea, m_aiConfigDock);
            m_aiConfigDock->show();
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
}

void MainWindow::InitializePresenter(
    mvc::IController& controller, db::EventsContainer& events)
{
    util::Logger::Debug("[MainWindow] InitializePresenter");
    m_searchResults->SetObserver(this);
    m_presenter = std::make_unique<ui::MainWindowPresenter>(*this, controller,
        events, *m_searchResults, m_eventsView, m_typeFilterView,
        m_itemDetailsView);
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
                    UpdateStatusText(QString("Loading %1 ...").arg(path).toStdString());
                    m_presenter->LoadLogFile(filePath);
                    m_presenter->SetItemDetailsVisible(true);
                    UpdateStatusText(QString("Data ready. Path: %1").arg(path).toStdString());
                }
                else // Merge
                {
                    // Merge with existing data
                    const std::string existingAlias = dialog.GetExistingFileAlias().toStdString();
                    const std::string newFileAlias = dialog.GetNewFileAlias().toStdString();
                    UpdateStatusText(QString("Merging %1 ...").arg(path).toStdString());
                    m_presenter->MergeLogFile(filePath, existingAlias, newFileAlias);
                    m_presenter->SetItemDetailsVisible(true);
                    UpdateStatusText(QString("Merge complete. Path: %1").arg(path).toStdString());
                }
            }
            // If dialog was canceled, do nothing
        }
        else
        {
            // No existing data, just load normally
            UpdateStatusText(QString("Loading %1 ...").arg(path).toStdString());
            m_presenter->LoadLogFile(filePath);
            m_presenter->SetItemDetailsVisible(true);
            UpdateStatusText(QString("Data ready. Path: %1").arg(path).toStdString());
        }
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("[MainWindow] Failed to load file '{}': {}",
            filePath.string(), ex.what());
        UpdateStatusText(QString("Failed to load  complete file Path: %1").arg(path).toStdString());
        QMessageBox::critical(this, "File Drop Error",
            QString("Unable to load %1\n%2").arg(path).arg(ex.what()));
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
    dialog.setNameFilter(tr("Log files (*.log *.txt *.xml);;All files (*.*)"));
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
        m_eventsView->RefreshView();
    }
    
    if (m_itemDetailsView)
        m_itemDetailsView->RefreshView();
    
    // Refresh AI client if provider changed
    if (m_aiConfigPanel)
        m_aiConfigPanel->RefreshAIClient();
}

} // namespace ui::qt
