#include "ui/qt/MainWindow.hpp"

#include "db/EventsContainer.hpp"
#include "mvc/IControler.hpp"
#include "ui/MainWindowPresenter.hpp"
#include "ui/qt/EventsTableView.hpp"
#include "ui/qt/ItemDetailsView.hpp"
#include "ui/qt/SearchResultsView.hpp"
#include "ui/qt/TypeFilterView.hpp"

#include <QApplication>
#include <QCoreApplication>
#include <QDragEnterEvent>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
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

#include <filesystem>
#include <stdexcept>

namespace ui::qt
{

MainWindow::MainWindow(mvc::IController& controller,
    db::EventsContainer& events, QWidget* parent)
    : QMainWindow(parent)
{
    InitializeUi(events);
    InitializePresenter(controller, events);
}

MainWindow::~MainWindow() = default;

void MainWindow::InitializeUi(db::EventsContainer& events)
{
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
    filtersLayout->addWidget(new QLabel("Extended filters placeholder",
        filtersTab));
    filtersLayout->addStretch(1);
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

    // Right content splitter (events list + details)
    m_eventsView = new EventsTableView(events, m_rightSplitter);

    m_itemDetailsView = new ItemDetailsView(events, m_rightSplitter);

    m_rightSplitter->addWidget(m_eventsView);
    m_rightSplitter->addWidget(m_itemDetailsView);
    m_rightSplitter->setStretchFactor(0, 3);
    m_rightSplitter->setStretchFactor(1, 2);

    m_leftSplitter->addWidget(leftPanel);
    m_leftSplitter->addWidget(m_rightSplitter);
    m_leftSplitter->setStretchFactor(0, 1);
    m_leftSplitter->setStretchFactor(1, 3);

    // Search panel mirrors wx bottom splitter
    auto* searchPanel = new QWidget(m_bottomSplitter);
    auto* searchLayout = new QVBoxLayout(searchPanel);
    searchLayout->setContentsMargins(8, 8, 8, 8);
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

    m_bottomSplitter->addWidget(m_leftSplitter);
    m_bottomSplitter->addWidget(searchPanel);
    m_bottomSplitter->setStretchFactor(0, 3);
    m_bottomSplitter->setStretchFactor(1, 1);

    setCentralWidget(m_bottomSplitter);
    setAcceptDrops(true);
    setWindowTitle("LogViewer Qt");
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

    if (m_eventsView && m_itemDetailsView)
    {
        connect(m_eventsView, &EventsTableView::CurrentActualRowChanged,
            m_itemDetailsView, &ItemDetailsView::OnActualRowChanged);
    }
}

void MainWindow::InitializePresenter(
    mvc::IController& controller, db::EventsContainer& events)
{
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
    QMessageBox::information(this, "Search Result",
        QString("Activated event ID: %1").arg(eventId));
}

void MainWindow::OnSearchRequested()
{
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

} // namespace ui::qt
