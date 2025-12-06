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
#include "ui/qt/AIChatPanel.hpp"
#include "ui/qt/OllamaSetupDialog.hpp"
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
#include <QSplitter>
#include <QStatusBar>
#include <QString>
#include <QTabWidget>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidget>
#include <QKeySequence>

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
}

MainWindow::~MainWindow() = default;

void MainWindow::InitializeUi(db::EventsContainer& events)
{
    util::Logger::Debug("[MainWindow] InitializeUi: events size={}",
        events.Size());
    m_bottomSplitter = new QSplitter(Qt::Vertical, this);
    m_leftSplitter = new QSplitter(Qt::Horizontal, m_bottomSplitter);
    m_rightSplitter = new QSplitter(Qt::Horizontal, m_leftSplitter);

    // Left filters panel
    auto* leftPanel = new QWidget(m_leftSplitter);
    auto* leftLayout = new QVBoxLayout(leftPanel);
    leftLayout->setContentsMargins(0, 0, 0, 0);
    leftLayout->setSpacing(8);

    m_filterTabs = new QTabWidget(leftPanel);

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

    leftLayout->addWidget(m_filterTabs);
    leftPanel->setLayout(leftLayout);

    // Right content area with tabs for events view and AI analysis
    auto* rightPanel = new QWidget(m_rightSplitter);
    auto* rightLayout = new QVBoxLayout(rightPanel);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    
    auto* contentTabs = new QTabWidget(rightPanel);
    
    // Events view tab
    m_eventsView = new EventsTableView(events, contentTabs);
    contentTabs->addTab(m_eventsView, "Events");
    
    // AI Analysis tab - use factory to create appropriate client
    auto& config = config::GetConfig();
    auto aiService = ai::AIServiceFactory::CreateClient(
        config.aiProvider,
        config.aiApiKey,
        config.ollamaBaseUrl,
        config.ollamaDefaultModel
    );
    auto aiAnalyzer = std::make_shared<ai::LogAnalyzer>(aiService, events);
    m_aiPanel = new AIAnalysisPanel(aiService, aiAnalyzer, contentTabs);
    contentTabs->addTab(m_aiPanel, "AI Analysis");
    
    rightLayout->addWidget(contentTabs);
    rightPanel->setLayout(rightLayout);

    m_itemDetailsView = new ItemDetailsView(events, m_rightSplitter);

    m_rightSplitter->addWidget(rightPanel);
    m_rightSplitter->addWidget(m_itemDetailsView);
    m_rightSplitter->setStretchFactor(0, 3);
    m_rightSplitter->setStretchFactor(1, 2);

    m_leftSplitter->addWidget(leftPanel);
    m_leftSplitter->addWidget(m_rightSplitter);
    m_leftSplitter->setStretchFactor(0, 1);
    m_leftSplitter->setStretchFactor(1, 3);

    // Bottom panel with tabs for Search and AI Chat
    auto* bottomTabs = new QTabWidget(m_bottomSplitter);
    
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
    
    // AI Chat tab - pass events container directly for context
    auto* chatPanel = new AIChatPanel(aiService, events, bottomTabs);
    bottomTabs->addTab(chatPanel, "AI Chat");

    m_bottomSplitter->addWidget(m_leftSplitter);
    m_bottomSplitter->addWidget(bottomTabs);
    m_bottomSplitter->setStretchFactor(0, 3);
    m_bottomSplitter->setStretchFactor(1, 1);

    setCentralWidget(m_bottomSplitter);
    setAcceptDrops(true);
    const auto title = QStringLiteral("LogViewer Qt %1")
                           .arg(QString::fromStdString(Version::current().asShortStr()));
    setWindowTitle(title);
    setMinimumSize(1024, 768);

    m_statusLabel = new QLabel("Ready", this);
    statusBar()->addWidget(m_statusLabel, 1);

    m_progressBar = new QProgressBar(this);
    m_progressBar->setVisible(false);
    m_progressBar->setTextVisible(false);
    m_progressBar->setFixedHeight(12);
    statusBar()->addPermanentWidget(m_progressBar, 0);

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
        UpdateStatusText(QString("Loading %1 ...").arg(path).toStdString());
        m_presenter->LoadLogFile(filePath);
        m_presenter->SetItemDetailsVisible(true);
        UpdateStatusText(QString("Data ready. Path: %1").arg(path).toStdString());
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("[MainWindow] Failed to load file '{}': {}",
            filePath.string(), ex.what());
        UpdateStatusText("Failed to load file");
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
    dialog.setOption(QFileDialog::DontUseNativeDialog, true);
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
}

} // namespace ui::qt
