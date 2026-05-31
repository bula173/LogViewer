#include "SideBySidePanel.hpp"

#include "events/EventsTableView.hpp"
#include "ParserFactory.hpp"
#include "Logger.hpp"

#include <QComboBox>
#include <QFileDialog>
#include <QFuture>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSplitter>
#include <QStandardPaths>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <cmath>
#include <limits>

namespace ui::qt
{

// ────────────────────────────────────────────────────────────────────────────
// Construction
// ────────────────────────────────────────────────────────────────────────────

SideBySidePanel::SideBySidePanel(QWidget* parent)
    : QWidget(parent)
{
    BuildUi();

    m_leftWatcher  = new QFutureWatcher<void>(this);
    m_rightWatcher = new QFutureWatcher<void>(this);

    connect(m_leftWatcher,  &QFutureWatcher<void>::finished,
            this, &SideBySidePanel::OnLeftLoadFinished);
    connect(m_rightWatcher, &QFutureWatcher<void>::finished,
            this, &SideBySidePanel::OnRightLoadFinished);
}

SideBySidePanel::~SideBySidePanel() = default;

// ────────────────────────────────────────────────────────────────────────────
// UI construction
// ────────────────────────────────────────────────────────────────────────────

void SideBySidePanel::BuildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(4, 4, 4, 4);
    root->setSpacing(4);

    // ── Sync toolbar ────────────────────────────────────────────────────────
    auto* toolbar = new QWidget(this);
    auto* tbLayout = new QHBoxLayout(toolbar);
    tbLayout->setContentsMargins(0, 0, 0, 0);
    tbLayout->setSpacing(6);

    tbLayout->addWidget(new QLabel(tr("Sync:"), toolbar));

    m_syncModeCombo = new QComboBox(toolbar);
    m_syncModeCombo->addItem(tr("Timestamp"),  static_cast<int>(SyncMode::Timestamp));
    m_syncModeCombo->addItem(tr("Manual"),     static_cast<int>(SyncMode::Manual));
    m_syncModeCombo->addItem(tr("None"),       static_cast<int>(SyncMode::None));
    tbLayout->addWidget(m_syncModeCombo);

    m_setLeftRefBtn  = new QPushButton(tr("Set Left Ref"), toolbar);
    m_setRightRefBtn = new QPushButton(tr("Set Right Ref"), toolbar);
    m_setLeftRefBtn->setToolTip(
        tr("Mark the currently selected left event as the synchronisation reference.\n"
           "Use this when the two files have different time bases."));
    m_setRightRefBtn->setToolTip(
        tr("Mark the currently selected right event as the synchronisation reference."));
    tbLayout->addWidget(m_setLeftRefBtn);
    tbLayout->addWidget(m_setRightRefBtn);

    m_syncStatusLabel = new QLabel(this);
    m_syncStatusLabel->setStyleSheet("color: gray;");
    tbLayout->addWidget(m_syncStatusLabel, 1);

    root->addWidget(toolbar);

    // ── Side-by-side splitter ────────────────────────────────────────────────
    auto* splitter = new QSplitter(Qt::Horizontal, this);
    splitter->setChildrenCollapsible(false);
    splitter->addWidget(BuildSide(true));
    splitter->addWidget(BuildSide(false));
    splitter->setSizes({500, 500});
    root->addWidget(splitter, 1);

    // ── Signal wiring ────────────────────────────────────────────────────────
    connect(m_syncModeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &SideBySidePanel::OnSyncModeChanged);
    connect(m_setLeftRefBtn,  &QPushButton::clicked, this, &SideBySidePanel::OnSetLeftRef);
    connect(m_setRightRefBtn, &QPushButton::clicked, this, &SideBySidePanel::OnSetRightRef);

    connect(m_leftView,  &EventsTableView::CurrentActualRowChanged,
            this, &SideBySidePanel::OnLeftSelectionChanged);
    connect(m_rightView, &EventsTableView::CurrentActualRowChanged,
            this, &SideBySidePanel::OnRightSelectionChanged);

    connect(m_leftOpenBtn,  &QPushButton::clicked, this, &SideBySidePanel::OnOpenLeftFile);
    connect(m_rightOpenBtn, &QPushButton::clicked, this, &SideBySidePanel::OnOpenRightFile);

    UpdateManualControls();
    UpdateSyncStatus();
}

QWidget* SideBySidePanel::BuildSide(bool isLeft)
{
    auto* container = new QWidget(this);
    auto* layout    = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(2);

    // Header row: open button + file label
    auto* header     = new QWidget(container);
    auto* headerLy   = new QHBoxLayout(header);
    headerLy->setContentsMargins(0, 0, 0, 0);

    auto* openBtn = new QPushButton(tr("Open…"), header);
    openBtn->setMaximumWidth(70);
    auto* fileLabel = new QLabel(tr("(no file)"), header);
    fileLabel->setStyleSheet("color: gray; font-style: italic;");
    headerLy->addWidget(openBtn);
    headerLy->addWidget(fileLabel, 1);
    layout->addWidget(header);

    // Table view backed by the side's container
    db::EventsContainer& evts = isLeft ? m_leftEvents : m_rightEvents;
    auto* view = new EventsTableView(evts, container);
    layout->addWidget(view, 1);

    if (isLeft) {
        m_leftOpenBtn   = openBtn;
        m_leftFileLabel = fileLabel;
        m_leftView      = view;
    } else {
        m_rightOpenBtn   = openBtn;
        m_rightFileLabel = fileLabel;
        m_rightView      = view;
    }

    return container;
}

// ────────────────────────────────────────────────────────────────────────────
// Toolbar state helpers
// ────────────────────────────────────────────────────────────────────────────

void SideBySidePanel::UpdateManualControls()
{
    const bool manual = (m_syncMode == SyncMode::Manual);
    m_setLeftRefBtn->setVisible(manual);
    m_setRightRefBtn->setVisible(manual);
}

void SideBySidePanel::UpdateSyncStatus()
{
    if (m_syncMode == SyncMode::None) {
        m_syncStatusLabel->setText(tr("Independent — no synchronisation"));
        return;
    }
    if (m_syncMode == SyncMode::Timestamp) {
        m_syncStatusLabel->setText(tr("Syncing by timestamp"));
        return;
    }
    // Manual
    if (!m_leftRefSet && !m_rightRefSet) {
        m_syncStatusLabel->setText(tr("Set both reference events to enable sync"));
    } else if (!m_leftRefSet) {
        m_syncStatusLabel->setText(tr("Left ref not set"));
    } else if (!m_rightRefSet) {
        m_syncStatusLabel->setText(tr("Right ref not set"));
    } else {
        const double offsetSec = m_syncOffset;
        m_syncStatusLabel->setText(
            tr("Offset: %1 s  (right = left + offset)")
                .arg(offsetSec, 0, 'f', 3));
    }
}

// ────────────────────────────────────────────────────────────────────────────
// Slots — toolbar
// ────────────────────────────────────────────────────────────────────────────

void SideBySidePanel::OnSyncModeChanged(int /*index*/)
{
    const int data = m_syncModeCombo->currentData().toInt();
    m_syncMode = static_cast<SyncMode>(data);
    UpdateManualControls();
    UpdateSyncStatus();
}

void SideBySidePanel::OnSetLeftRef()
{
    const int row = m_leftView->CurrentActualRow();
    if (row < 0 || m_leftEvents.Size() == 0) {
        QMessageBox::information(this, tr("Side by Side"),
            tr("Select an event in the left panel first."));
        return;
    }
    m_leftRefRow = row;
    m_leftRefTs  = GetTimestamp(m_leftEvents, static_cast<size_t>(row));
    m_leftRefSet = true;

    if (m_rightRefSet) {
        m_syncOffset = m_rightRefTs - m_leftRefTs;
        util::Logger::Info("[SideBySide] Manual sync offset = {:.6f} s", m_syncOffset);
    }
    UpdateSyncStatus();
}

void SideBySidePanel::OnSetRightRef()
{
    const int row = m_rightView->CurrentActualRow();
    if (row < 0 || m_rightEvents.Size() == 0) {
        QMessageBox::information(this, tr("Side by Side"),
            tr("Select an event in the right panel first."));
        return;
    }
    m_rightRefRow = row;
    m_rightRefTs  = GetTimestamp(m_rightEvents, static_cast<size_t>(row));
    m_rightRefSet = true;

    if (m_leftRefSet) {
        m_syncOffset = m_rightRefTs - m_leftRefTs;
        util::Logger::Info("[SideBySide] Manual sync offset = {:.6f} s", m_syncOffset);
    }
    UpdateSyncStatus();
}

// ────────────────────────────────────────────────────────────────────────────
// Slots — file open
// ────────────────────────────────────────────────────────────────────────────

void SideBySidePanel::OnOpenLeftFile()  { LoadFile(true);  }
void SideBySidePanel::OnOpenRightFile() { LoadFile(false); }

void SideBySidePanel::OpenLeft(const QString& path)  { LoadFileDirect(true,  path); }
void SideBySidePanel::OpenRight(const QString& path) { LoadFileDirect(false, path); }

void SideBySidePanel::LoadFile(bool isLeft)
{
    // Prevent loading while another load is active on the same side.
    auto* watcher = isLeft ? m_leftWatcher : m_rightWatcher;
    if (watcher->isRunning()) return;

    const QString docs =
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString path = QFileDialog::getOpenFileName(
        this,
        isLeft ? tr("Open Left Log File") : tr("Open Right Log File"),
        docs,
        tr("Log files (*.log *.txt *.xml *.csv *.json *.jsonl *.asc *.dlt *.evl);;"
           "All files (*.*)"));

    if (path.isEmpty()) return;

    const std::filesystem::path fspath(path.toStdString());
    auto result = parser::ParserFactory::CreateFromFile(fspath);
    if (!result.isOk()) {
        QMessageBox::critical(this, tr("Side by Side"),
            tr("Cannot open file:\n%1").arg(
                QString::fromStdString(result.error().what())));
        return;
    }

    StartAsyncLoad(isLeft, result.unwrap(), fspath);

    QLabel* lbl = isLeft ? m_leftFileLabel : m_rightFileLabel;
    lbl->setText(QFileInfo(path).fileName());
    lbl->setStyleSheet("color: orange; font-style: normal;");
    lbl->setToolTip(path);
    (isLeft ? m_leftOpenBtn : m_rightOpenBtn)->setEnabled(false);
}

void SideBySidePanel::LoadFileDirect(bool isLeft, const QString& path)
{
    auto* watcher = isLeft ? m_leftWatcher : m_rightWatcher;
    if (watcher->isRunning()) return;

    const std::filesystem::path fspath(path.toStdString());
    auto result = parser::ParserFactory::CreateFromFile(fspath);
    if (!result.isOk()) {
        QMessageBox::critical(this, tr("Side by Side"),
            tr("Cannot open file:\n%1").arg(
                QString::fromStdString(result.error().what())));
        return;
    }

    QLabel* lbl = isLeft ? m_leftFileLabel : m_rightFileLabel;
    lbl->setText(QFileInfo(path).fileName());
    lbl->setStyleSheet("color: orange; font-style: normal;");
    lbl->setToolTip(path);
    (isLeft ? m_leftOpenBtn : m_rightOpenBtn)->setEnabled(false);

    StartAsyncLoad(isLeft, result.unwrap(), fspath);
}

void SideBySidePanel::StartAsyncLoad(bool isLeft,
    std::unique_ptr<parser::IDataParser> parser,
    const std::filesystem::path& path)
{
    db::EventsContainer& container = isLeft ? m_leftEvents : m_rightEvents;
    container.Clear();

    // Reset manual refs when a side is reloaded.
    if (isLeft)  { m_leftRefSet  = false; }
    else         { m_rightRefSet = false; }
    m_syncOffset = 0.0;
    UpdateSyncStatus();

    LoadJob& job = isLeft ? m_leftJob : m_rightJob;
    job.parser   = std::move(parser);
    job.observer = std::make_unique<LoadJob::Observer>(container);

    job.parser->RegisterObserver(job.observer.get());

    // Suspend per-batch notifications; one RefreshView() after loading is enough.
    container.SuspendNotifications();

    auto* rawParser   = job.parser.get();
    auto* rawObserver = job.observer.get();

    auto* fw = isLeft ? m_leftWatcher : m_rightWatcher;
    fw->setFuture(QtConcurrent::run(
        [rawParser, rawObserver, path]()
        {
            try {
                rawParser->ParseData(path);
            } catch (...) {
                rawObserver->exception = std::current_exception();
            }
        }));
}

void SideBySidePanel::OnLeftLoadFinished()
{
    m_leftEvents.ResumeNotifications();
    m_leftJob.parser->UnregisterObserver(m_leftJob.observer.get());

    if (m_leftJob.observer->exception) {
        try { std::rethrow_exception(m_leftJob.observer->exception); }
        catch (const std::exception& ex) {
            QMessageBox::critical(this, tr("Side by Side"),
                tr("Error loading left file:\n%1").arg(ex.what()));
        }
    }

    m_leftView->RefreshView();
    m_leftOpenBtn->setEnabled(true);
    m_leftFileLabel->setStyleSheet("color: black; font-style: normal;");
    UpdateSyncStatus();

    const size_t n = m_leftEvents.Size();
    m_leftFileLabel->setToolTip(
        m_leftFileLabel->toolTip() +
        tr("  (%1 events)").arg(static_cast<qulonglong>(n)));

    util::Logger::Info("[SideBySide] Left loaded: {} events", n);
}

void SideBySidePanel::OnRightLoadFinished()
{
    m_rightEvents.ResumeNotifications();
    m_rightJob.parser->UnregisterObserver(m_rightJob.observer.get());

    if (m_rightJob.observer->exception) {
        try { std::rethrow_exception(m_rightJob.observer->exception); }
        catch (const std::exception& ex) {
            QMessageBox::critical(this, tr("Side by Side"),
                tr("Error loading right file:\n%1").arg(ex.what()));
        }
    }

    m_rightView->RefreshView();
    m_rightOpenBtn->setEnabled(true);
    m_rightFileLabel->setStyleSheet("color: black; font-style: normal;");
    UpdateSyncStatus();

    const size_t n = m_rightEvents.Size();
    m_rightFileLabel->setToolTip(
        m_rightFileLabel->toolTip() +
        tr("  (%1 events)").arg(static_cast<qulonglong>(n)));

    util::Logger::Info("[SideBySide] Right loaded: {} events", n);
}

// ────────────────────────────────────────────────────────────────────────────
// Sync — selection change handlers
// ────────────────────────────────────────────────────────────────────────────

void SideBySidePanel::OnLeftSelectionChanged(int actualRow)
{
    if (m_syncInProgress || m_syncMode == SyncMode::None) return;
    if (actualRow < 0 || m_rightEvents.Size() == 0) return;
    SyncFromLeft(actualRow);
}

void SideBySidePanel::OnRightSelectionChanged(int actualRow)
{
    if (m_syncInProgress || m_syncMode == SyncMode::None) return;
    if (actualRow < 0 || m_leftEvents.Size() == 0) return;
    SyncFromRight(actualRow);
}

void SideBySidePanel::SyncFromLeft(int actualRow)
{
    if (m_syncMode == SyncMode::Manual && (!m_leftRefSet || !m_rightRefSet)) {
        // Not ready for timestamp sync yet; fall back to row-offset.
        const int delta = actualRow - m_leftRefRow;
        const int target = std::clamp(m_rightRefRow + delta,
                                      0, static_cast<int>(m_rightEvents.Size()) - 1);
        m_syncInProgress = true;
        m_rightView->ScrollToActualRow(target);
        m_syncInProgress = false;
        return;
    }

    const double ts = GetTimestamp(m_leftEvents, static_cast<size_t>(actualRow));
    // For Manual mode offset is already computed; Timestamp mode uses offset=0.
    const double target = ts + m_syncOffset;
    const int rightRow = FindNearestByTimestamp(m_rightEvents, target,
        static_cast<double>(actualRow - m_leftRefRow));

    if (rightRow < 0) return;
    m_syncInProgress = true;
    m_rightView->ScrollToActualRow(rightRow);
    m_syncInProgress = false;
}

void SideBySidePanel::SyncFromRight(int actualRow)
{
    if (m_syncMode == SyncMode::Manual && (!m_leftRefSet || !m_rightRefSet)) {
        const int delta = actualRow - m_rightRefRow;
        const int target = std::clamp(m_leftRefRow + delta,
                                      0, static_cast<int>(m_leftEvents.Size()) - 1);
        m_syncInProgress = true;
        m_leftView->ScrollToActualRow(target);
        m_syncInProgress = false;
        return;
    }

    const double ts = GetTimestamp(m_rightEvents, static_cast<size_t>(actualRow));
    const double target = ts - m_syncOffset;
    const int leftRow = FindNearestByTimestamp(m_leftEvents, target,
        static_cast<double>(actualRow - m_rightRefRow));

    if (leftRow < 0) return;
    m_syncInProgress = true;
    m_leftView->ScrollToActualRow(leftRow);
    m_syncInProgress = false;
}

// ────────────────────────────────────────────────────────────────────────────
// Sync helpers
// ────────────────────────────────────────────────────────────────────────────

double SideBySidePanel::GetTimestamp(db::EventsContainer& container, size_t row) const
{
    if (container.Size() == 0) return -1.0;
    const std::string ts = container.GetEvent(row).findByKey("timestamp");
    if (ts.empty()) return -1.0;
    bool ok = false;
    const double v = QString::fromStdString(ts).toDouble(&ok);
    return ok ? v : -1.0;
}

int SideBySidePanel::FindNearestByTimestamp(db::EventsContainer& container,
    double target, double fallbackRowDelta) const
{
    const size_t total = container.Size();
    if (total == 0) return -1;

    // Try timestamp-based lookup: linear scan (handles non-sorted logs correctly;
    // typically O(n) but n is usually small relative to display rate).
    double bestDiff = std::numeric_limits<double>::max();
    int    bestRow  = -1;
    bool   anyTs    = false;

    for (size_t i = 0; i < total; ++i) {
        const std::string ts = container.GetEvent(i).findByKey("timestamp");
        if (ts.empty()) continue;
        bool ok = false;
        const double t = QString::fromStdString(ts).toDouble(&ok);
        if (!ok) continue;
        anyTs = true;
        const double diff = std::fabs(t - target);
        if (diff < bestDiff) {
            bestDiff = diff;
            bestRow  = static_cast<int>(i);
        }
    }

    if (anyTs) return bestRow;

    // No parseable timestamps: fall back to index offset from the reference rows.
    const int fallback = static_cast<int>(std::llround(fallbackRowDelta));
    return std::clamp(fallback, 0, static_cast<int>(total) - 1);
}

} // namespace ui::qt
