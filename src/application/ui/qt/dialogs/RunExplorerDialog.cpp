#include "RunExplorerDialog.hpp"

#include <QAction>
#include <QBrush>
#include <QCheckBox>
#include <QColor>
#include <QComboBox>
#include <QDir>
#include <QFont>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QProgressBar>
#include <QPromise>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrentRun>

#include <algorithm>
#include <filesystem>
#include <utility>

namespace ui::qt
{

namespace
{

using analyzer::TestRunEntry;
using analyzer::TestVerdict;

enum Column
{
    ColOrder,
    ColVerdict,
    ColTest,
    ColDuration,
    ColEntries,
    ColMessage,
    ColFile,
    ColumnCount
};

constexpr int kIndexRole = Qt::UserRole;     ///< entry index; -1 on a group row
constexpr int kSortRole  = Qt::UserRole + 1; ///< numeric sort key of a column

/// Sorts by kSortRole when both rows carry one, else by text.
class Item : public QTreeWidgetItem
{
  public:
    using QTreeWidgetItem::QTreeWidgetItem;

    bool operator<(const QTreeWidgetItem& other) const override
    {
        const int      column = treeWidget() ? treeWidget()->sortColumn() : 0;
        const QVariant a      = data(column, kSortRole);
        const QVariant b      = other.data(column, kSortRole);
        if (a.isValid() && b.isValid())
            return a.toLongLong() < b.toLongLong();
        return text(column).localeAwareCompare(other.text(column)) < 0;
    }
};

QString VerdictText(TestVerdict verdict)
{
    switch (verdict)
    {
        case TestVerdict::Pass: return QStringLiteral("PASS");
        case TestVerdict::Fail: return QStringLiteral("FAIL");
        default:                return QStringLiteral("?");
    }
}

QColor VerdictColor(TestVerdict verdict)
{
    // Mid tones (as in the Test Steps tab), readable on light and dark themes.
    return verdict == TestVerdict::Fail ? QColor(0xd3, 0x2f, 0x2f)
         : verdict == TestVerdict::Pass ? QColor(0x2e, 0x7d, 0x32)
                                        : QColor(0x9e, 0x9e, 0x9e);
}

QString DurationText(std::int64_t us)
{
    return us < 0 ? QString{} : QStringLiteral("%1 s").arg(static_cast<double>(us) / 1e6, 0, 'f', 1);
}

QString ToQString(const std::filesystem::path& path)
{
    return QString::fromStdString(path.string());
}

} // namespace

RunExplorerDialog::RunExplorerDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Run Explorer"));
    setModal(false);
    resize(1100, 560);

    m_summary = new QLabel(tr("Open a results folder to list its tests."), this);
    m_summary->setObjectName("runExplorerSummary");
    m_summary->setTextInteractionFlags(Qt::TextSelectableByMouse);

    m_verdictFilter = new QComboBox(this);
    m_verdictFilter->setObjectName("runExplorerVerdictFilter");
    m_verdictFilter->addItem(tr("All tests"), -1);
    m_verdictFilter->addItem(tr("Failed"), static_cast<int>(TestVerdict::Fail));
    m_verdictFilter->addItem(tr("Passed"), static_cast<int>(TestVerdict::Pass));

    m_groupByMessage = new QCheckBox(tr("Group failures by message"), this);
    m_groupByMessage->setObjectName("runExplorerGroupByMessage");
    m_groupByMessage->setToolTip(tr("Failures whose messages differ only in numbers collapse into one row"));

    m_progress = new QProgressBar(this);
    m_progress->setObjectName("runExplorerProgress");
    m_progress->setMaximumWidth(220);
    m_progress->hide();
    m_cancelButton = new QPushButton(tr("Cancel"), this);
    m_cancelButton->setObjectName("runExplorerCancel");
    m_cancelButton->hide();

    auto* top = new QHBoxLayout;
    top->addWidget(m_summary, 1);
    top->addWidget(m_progress);
    top->addWidget(m_cancelButton);
    top->addWidget(new QLabel(tr("Show:"), this));
    top->addWidget(m_verdictFilter);
    top->addWidget(m_groupByMessage);

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName("runExplorerTree");
    m_tree->setColumnCount(ColumnCount);
    m_tree->setHeaderLabels({tr("#"), tr("Verdict"), tr("Test"), tr("Duration"), tr("Entries"),
                             tr("Failure message"), tr("File")});
    m_tree->setRootIsDecorated(false);
    m_tree->setUniformRowHeights(true);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(ColOrder, Qt::AscendingOrder);
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(ColMessage, QHeaderView::Stretch);
    m_tree->setColumnWidth(ColOrder, 40);
    m_tree->setColumnWidth(ColVerdict, 60);
    m_tree->setColumnWidth(ColTest, 320);
    m_tree->setColumnWidth(ColDuration, 70);
    m_tree->setColumnWidth(ColEntries, 70);
    m_tree->setColumnWidth(ColFile, 200);

    m_openAction = new QAction(tr("Open"), this);
    m_openAction->setObjectName("runExplorerOpen");
    m_openAction->setToolTip(tr("Load this test's log, replacing the current data"));
    m_openWithPreviousAction = new QAction(tr("Open with Previous Test"), this);
    m_openWithPreviousAction->setObjectName("runExplorerOpenWithPrevious");
    m_openWithPreviousAction->setToolTip(
        tr("Load this test merged with the test that ran just before it, e.g. to see a teardown that broke it"));
    m_tree->setContextMenuPolicy(Qt::ActionsContextMenu);
    m_tree->addAction(m_openAction);
    m_tree->addAction(m_openWithPreviousAction);

    m_openButton = new QPushButton(m_openAction->text(), this);
    m_openButton->setToolTip(m_openAction->toolTip());
    m_openWithPreviousButton = new QPushButton(m_openWithPreviousAction->text(), this);
    m_openWithPreviousButton->setToolTip(m_openWithPreviousAction->toolTip());
    auto* closeButton = new QPushButton(tr("Close"), this);

    auto* bottom = new QHBoxLayout;
    bottom->addWidget(new QLabel(tr("Double-click a test to open its log."), this), 1);
    bottom->addWidget(m_openButton);
    bottom->addWidget(m_openWithPreviousButton);
    bottom->addWidget(closeButton);

    auto* layout = new QVBoxLayout(this);
    layout->addLayout(top);
    layout->addWidget(m_tree, 1);
    layout->addLayout(bottom);

    m_watcher = new QFutureWatcher<std::vector<TestRunEntry>>(this);
    connect(m_watcher, &QFutureWatcherBase::progressRangeChanged, m_progress, &QProgressBar::setRange);
    connect(m_watcher, &QFutureWatcherBase::progressValueChanged, m_progress, &QProgressBar::setValue);
    connect(m_watcher, &QFutureWatcherBase::finished, this, &RunExplorerDialog::OnScanFinished);
    connect(m_cancelButton, &QPushButton::clicked, m_watcher, &QFutureWatcherBase::cancel);

    connect(m_verdictFilter, &QComboBox::currentIndexChanged, this, &RunExplorerDialog::ApplyVerdictFilter);
    connect(m_groupByMessage, &QCheckBox::toggled, this, &RunExplorerDialog::Rebuild);
    connect(m_tree, &QTreeWidget::currentItemChanged, this, &RunExplorerDialog::UpdateActions);
    connect(m_tree, &QTreeWidget::itemActivated, this, [this](QTreeWidgetItem* item) {
        if (item && item->data(0, kIndexRole).toInt() >= 0)
            OpenCurrent();
    });
    connect(m_openAction, &QAction::triggered, this, &RunExplorerDialog::OpenCurrent);
    connect(m_openWithPreviousAction, &QAction::triggered, this, &RunExplorerDialog::OpenCurrentWithPrevious);
    connect(m_openButton, &QPushButton::clicked, m_openAction, &QAction::trigger);
    connect(m_openWithPreviousButton, &QPushButton::clicked, m_openWithPreviousAction, &QAction::trigger);
    connect(closeButton, &QPushButton::clicked, this, &QDialog::close);

    UpdateActions();
}

RunExplorerDialog::~RunExplorerDialog()
{
    m_watcher->cancel();
    m_watcher->waitForFinished(); // stops after the log being read
}

void RunExplorerDialog::OpenFolder(const QString& folder)
{
    m_watcher->cancel();
    m_watcher->waitForFinished();

    m_folder = folder;
    m_entries.clear();
    Rebuild();
    setWindowTitle(tr("Run Explorer - %1").arg(QDir(folder).dirName()));
    m_summary->setText(tr("Scanning %1 ...").arg(QDir::toNativeSeparators(folder)));
    m_progress->setRange(0, 0);
    m_progress->show();
    m_cancelButton->show();

    const std::filesystem::path path(folder.toStdString());
    m_watcher->setFuture(QtConcurrent::run([path](QPromise<std::vector<TestRunEntry>>& promise) {
        auto entries = analyzer::ScanResultsFolder(path, [&promise](std::size_t done, std::size_t total) {
            promise.setProgressRange(0, static_cast<int>(total));
            promise.setProgressValue(static_cast<int>(done));
            return !promise.isCanceled();
        });
        if (!promise.isCanceled())
            promise.addResult(std::move(entries));
    }));
}

void RunExplorerDialog::OnScanFinished()
{
    m_progress->hide();
    m_cancelButton->hide();
    if (m_watcher->isCanceled() || m_watcher->future().resultCount() == 0)
        m_summary->setText(tr("Scan of %1 cancelled.").arg(QDir::toNativeSeparators(m_folder)));
    else
        SetEntries(m_folder, m_watcher->result());
    emit ScanFinished();
}

void RunExplorerDialog::SetEntries(const QString& folder, std::vector<TestRunEntry> entries)
{
    m_folder  = folder;
    m_entries = std::move(entries);

    const auto failed = std::count_if(m_entries.begin(), m_entries.end(),
                                      [](const TestRunEntry& e) { return e.verdict == TestVerdict::Fail; });
    m_summary->setText(m_entries.empty()
        ? tr("No *__merged_logs.txt test logs in %1").arg(QDir::toNativeSeparators(folder))
        : tr("%1: %2 tests, %3 failed").arg(QDir::toNativeSeparators(folder))
              .arg(m_entries.size()).arg(failed));
    Rebuild();
}

QTreeWidgetItem* RunExplorerDialog::CreateTestItem(int index) const
{
    const TestRunEntry& e    = m_entries[static_cast<std::size_t>(index)];
    auto*               item = new Item;
    item->setData(0, kIndexRole, index);

    item->setText(ColOrder, QString::number(index + 1));
    item->setData(ColOrder, kSortRole, index);
    item->setText(ColVerdict, VerdictText(e.verdict));
    item->setForeground(ColVerdict, QBrush(VerdictColor(e.verdict)));
    item->setText(ColTest, QString::fromStdString(e.testName));
    item->setText(ColDuration, DurationText(e.durationUs));
    item->setData(ColDuration, kSortRole, static_cast<qlonglong>(e.durationUs));
    item->setToolTip(ColDuration, tr("Time covered by the log (first to last event)"));
    if (e.entries >= 0)
        item->setText(ColEntries, QString::number(e.entries));
    item->setData(ColEntries, kSortRole, static_cast<qlonglong>(e.entries));
    const QString message = QString::fromStdString(e.failureMessage);
    item->setText(ColMessage, message);
    if (!message.isEmpty())
    {
        item->setToolTip(ColMessage, e.failureSource == analyzer::FailureSource::StepFail
            ? tr("%1\n\nFrom the decisive STEP_FAIL of the log").arg(message)
            : tr("%1\n\nFrom xunit.xml / output.xml (the log records no failing step)").arg(message));
    }
    item->setText(ColFile, ToQString(e.file.filename()));
    item->setToolTip(ColFile, QDir::toNativeSeparators(ToQString(e.file)));
    return item;
}

void RunExplorerDialog::Rebuild()
{
    m_tree->setSortingEnabled(false);
    m_tree->clear();

    const bool grouped = m_groupByMessage->isChecked();
    m_tree->setRootIsDecorated(grouped);

    std::vector<bool> inGroup(m_entries.size(), false);
    if (grouped)
    {
        for (const auto& group : analyzer::GroupFailures(m_entries))
        {
            auto* parent = new Item(m_tree);
            parent->setData(0, kIndexRole, -1);
            parent->setData(ColOrder, kSortRole, static_cast<qlonglong>(group.members.front()));
            parent->setText(ColVerdict, VerdictText(TestVerdict::Fail));
            parent->setForeground(ColVerdict, QBrush(VerdictColor(TestVerdict::Fail)));
            parent->setText(ColTest, tr("%1 × same failure").arg(group.members.size()));
            parent->setText(ColMessage, QString::fromStdString(group.message));
            parent->setToolTip(ColMessage, QString::fromStdString(group.message));
            QFont bold = parent->font(ColTest);
            bold.setBold(true);
            parent->setFont(ColTest, bold);
            for (const std::size_t member : group.members)
            {
                parent->addChild(CreateTestItem(static_cast<int>(member)));
                inGroup[member] = true;
            }
        }
    }
    for (std::size_t i = 0; i < m_entries.size(); ++i)
    {
        if (!inGroup[i])
            m_tree->addTopLevelItem(CreateTestItem(static_cast<int>(i)));
    }

    m_tree->setSortingEnabled(true);
    ApplyVerdictFilter();
    UpdateActions();
}

void RunExplorerDialog::ApplyVerdictFilter()
{
    const int  wanted = m_verdictFilter->currentData().toInt();
    const auto hidden = [this, wanted](const QTreeWidgetItem* item) {
        const int index = item->data(0, kIndexRole).toInt();
        return wanted >= 0 && index >= 0 &&
               static_cast<int>(m_entries[static_cast<std::size_t>(index)].verdict) != wanted;
    };
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* top = m_tree->topLevelItem(i);
        if (top->childCount() == 0)
        {
            top->setHidden(hidden(top));
            continue;
        }
        bool anyVisible = false;
        for (int c = 0; c < top->childCount(); ++c)
        {
            top->child(c)->setHidden(hidden(top->child(c)));
            anyVisible = anyVisible || !top->child(c)->isHidden();
        }
        top->setHidden(!anyVisible);
    }
}

int RunExplorerDialog::CurrentEntry() const
{
    const QTreeWidgetItem* item = m_tree->currentItem();
    return item ? item->data(0, kIndexRole).toInt() : -1;
}

void RunExplorerDialog::UpdateActions()
{
    const int index = CurrentEntry();
    m_openAction->setEnabled(index >= 0);
    m_openWithPreviousAction->setEnabled(index > 0);
    m_openWithPreviousAction->setStatusTip(
        index > 0 ? tr("Previous test: %1").arg(QString::fromStdString(
                        m_entries[static_cast<std::size_t>(index - 1)].testName))
                  : QString{});
    m_openButton->setEnabled(m_openAction->isEnabled());
    m_openWithPreviousButton->setEnabled(m_openWithPreviousAction->isEnabled());
}

void RunExplorerDialog::OpenCurrent()
{
    const int index = CurrentEntry();
    if (index >= 0)
        emit OpenTestRequested(ToQString(m_entries[static_cast<std::size_t>(index)].file));
}

void RunExplorerDialog::OpenCurrentWithPrevious()
{
    const int index = CurrentEntry();
    if (index <= 0)
        return;
    // Entries are in run order: the test before this one ran just before it.
    const TestRunEntry& previous = m_entries[static_cast<std::size_t>(index - 1)];
    const TestRunEntry& current  = m_entries[static_cast<std::size_t>(index)];
    emit OpenWithPreviousRequested(ToQString(previous.file), QString::fromStdString(previous.testName),
                                   ToQString(current.file), QString::fromStdString(current.testName));
}

} // namespace ui::qt
