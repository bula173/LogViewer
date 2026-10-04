#include "TestStepsPanel.hpp"

#include "EventsContainer.hpp"

#include <QBrush>
#include <QColor>
#include <QFont>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QPushButton>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace ui::qt {

namespace {

enum Column
{
    kStepColumn,
    kStartColumn,
    kDurationColumn,
    kEventsColumn,
    kDetailsColumn,
    kColumnCount
};

constexpr int kMaxBannerMessage = 300;

QString FormatDuration(std::int64_t us)
{
    if (us < 0)
        return {};
    if (us < 1'000)
        return QStringLiteral("%1 ms").arg(static_cast<double>(us) / 1'000.0, 0, 'f', 1);
    if (us < 1'000'000)
        return QStringLiteral("%1 ms").arg(us / 1'000);
    return QStringLiteral("%1 s").arg(static_cast<double>(us) / 1'000'000.0, 0, 'f', 1);
}

QString FormatStart(std::int64_t us)
{
    if (us < 0)
        return {};
    return QStringLiteral("+%1 s").arg(static_cast<double>(us) / 1'000'000.0, 0, 'f', 3);
}

QString StatusSymbol(analyzer::StepStatus status)
{
    switch (status)
    {
        case analyzer::StepStatus::Passed:    return QStringLiteral("✓");
        case analyzer::StepStatus::Recovered: return QStringLiteral("⟳");
        case analyzer::StepStatus::Failed:    return QStringLiteral("✗");
        case analyzer::StepStatus::NotRun:    return QStringLiteral("–");
    }
    return {};
}

QColor StatusColor(analyzer::StepStatus status)
{
    switch (status)
    {
        case analyzer::StepStatus::Passed:    return QColor(0x2e, 0x7d, 0x32);
        case analyzer::StepStatus::Recovered: return QColor(0xe6, 0x7e, 0x00);
        case analyzer::StepStatus::Failed:    return QColor(0xd3, 0x2f, 0x2f);
        case analyzer::StepStatus::NotRun:    return QColor(0x9e, 0x9e, 0x9e);
    }
    return {};
}

QString Details(const analyzer::TestStep& step)
{
    using analyzer::StepStatus;
    const QString message = QString::fromStdString(step.failMessage);
    if (step.kind == analyzer::TestStep::Kind::Section)
    {
        if (step.status == StepStatus::NotRun)
            return QObject::tr("probably not run (heuristic)");
        if (step.status == StepStatus::Recovered)
            return QObject::tr("%n failure(s), all recovered", nullptr, step.failures);
        if (step.status == StepStatus::Failed)
            return message;
        return {};
    }
    switch (step.status)
    {
        case StepStatus::NotRun:    return QObject::tr("probably not run (heuristic)");
        case StepStatus::Recovered: return QObject::tr("failed, recovered: %1").arg(message);
        case StepStatus::Failed:    return message;
        case StepStatus::Passed:    break;
    }
    return QString::fromStdString(step.args);
}

QTreeWidgetItem* MakeItem(const analyzer::TestStep& step)
{
    auto* item = new QTreeWidgetItem();
    item->setText(kStepColumn, StatusSymbol(step.status) + ' ' + QString::fromStdString(step.name));
    item->setText(kStartColumn, FormatStart(step.startUs));
    item->setText(kDurationColumn, FormatDuration(step.durationUs));
    item->setText(kEventsColumn, QString::number(step.endRow - step.firstRow));
    item->setText(kDetailsColumn, Details(step));
    item->setData(kStepColumn, Qt::UserRole, QVariant::fromValue<qulonglong>(step.firstRow));
    item->setToolTip(kStepColumn, QString::fromStdString(step.name));
    item->setToolTip(kDetailsColumn, item->text(kDetailsColumn));
    item->setTextAlignment(kStartColumn, Qt::AlignRight | Qt::AlignVCenter);
    item->setTextAlignment(kDurationColumn, Qt::AlignRight | Qt::AlignVCenter);
    item->setTextAlignment(kEventsColumn, Qt::AlignRight | Qt::AlignVCenter);

    const QBrush brush(StatusColor(step.status));
    item->setForeground(kStepColumn, brush);
    if (step.status != analyzer::StepStatus::Passed)
        item->setForeground(kDetailsColumn, brush);
    if (step.kind == analyzer::TestStep::Kind::Section)
    {
        QFont font = item->font(kStepColumn);
        font.setBold(true);
        item->setFont(kStepColumn, font);
    }
    return item;
}

} // namespace

TestStepsPanel::TestStepsPanel(db::EventsContainer& events, QWidget* parent)
    : QWidget(parent), m_events(events)
{
    BuildLayout();
    ShowOutline();
}

void TestStepsPanel::BuildLayout()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    m_hint = new QLabel(tr("No test steps in this log.\n\nThis tab outlines test-harness logs "
                           "(e.g. safeAPI merged test logs) that contain TEST_START … TEST_END "
                           "events: the steps, their durations and the step that failed the test."),
                        this);
    m_hint->setObjectName("testStepsHint");
    m_hint->setAlignment(Qt::AlignCenter);
    m_hint->setWordWrap(true);
    layout->addWidget(m_hint, 1);

    m_banner = new QFrame(this);
    m_banner->setObjectName("testStepsBanner");
    auto* bannerLayout = new QHBoxLayout(m_banner);
    bannerLayout->setContentsMargins(10, 6, 10, 6);
    m_bannerText = new QLabel(m_banner);
    m_bannerText->setObjectName("testStepsBannerText");
    m_bannerText->setWordWrap(true);
    m_bannerText->setTextInteractionFlags(Qt::TextSelectableByMouse);
    bannerLayout->addWidget(m_bannerText, 1);
    m_goToFailure = new QPushButton(tr("Go to failure"), m_banner);
    m_goToFailure->setObjectName("testStepsGoToFailure");
    m_goToFailure->setToolTip(tr("Show the event that failed the test in the Events tab"));
    bannerLayout->addWidget(m_goToFailure);
    layout->addWidget(m_banner);

    m_tree = new QTreeWidget(this);
    m_tree->setObjectName("testStepsTree");
    m_tree->setColumnCount(kColumnCount);
    m_tree->setHeaderLabels({tr("Step"), tr("Start"), tr("Duration"), tr("Events"), tr("Details")});
    m_tree->header()->setStretchLastSection(true);
    m_tree->header()->setSectionResizeMode(kStepColumn, QHeaderView::Interactive);
    m_tree->setColumnWidth(kStepColumn, 380);
    for (const int col : {kStartColumn, kDurationColumn, kEventsColumn})
        m_tree->header()->setSectionResizeMode(col, QHeaderView::ResizeToContents);
    m_tree->setAlternatingRowColors(true);
    m_tree->setUniformRowHeights(true);
    m_tree->setToolTip(tr("Double-click or press Enter to show the step's first event in the Events tab.\n"
                          "⟳ failed but recovered (retried, or the test passed anyway), ✗ failed, "
                          "– probably not run (heuristic).\n"
                          "Duration: until the next step starts. Setup: before the first TestStep marker; "
                          "Teardown: after the last TestExpectation marker."));
    m_tree->installEventFilter(this);
    layout->addWidget(m_tree, 1);

    connect(m_tree, &QTreeWidget::itemDoubleClicked, this,
            [this](QTreeWidgetItem* item, int) { ActivateItem(item); });
    connect(m_goToFailure, &QPushButton::clicked, this, [this]() {
        if (m_outline.decisiveFailureRow)
            emit NavigateToEvent(static_cast<int>(*m_outline.decisiveFailureRow));
    });
}

void TestStepsPanel::Refresh()
{
    const std::uint64_t generation = m_events.Generation();
    const std::size_t   size       = m_events.Size();
    auto                metadata   = m_events.GetFileMetadata();
    if (m_built && generation == m_builtGeneration && size == m_builtSize &&
        metadata == m_builtMetadata)
        return; // only the filter changed; the outline covers the whole log

    m_outline         = analyzer::BuildTestOutline(m_events);
    m_built           = true;
    m_builtGeneration = generation;
    m_builtSize       = size;
    m_builtMetadata   = std::move(metadata);
    ShowOutline();
}

void TestStepsPanel::ShowOutline()
{
    m_tree->clear();
    const bool hasTest = m_outline.HasTest();
    m_hint->setVisible(!hasTest);
    m_banner->setVisible(hasTest);
    m_tree->setVisible(hasTest);
    if (!hasTest)
        return;

    // ── Banner ────────────────────────────────────────────────────────────
    const QString name = QString::fromStdString(m_outline.testName);
    QString text;
    QString tooltip;
    QString style;
    switch (m_outline.verdict)
    {
        case analyzer::TestVerdict::Pass:
        {
            int recovered = 0;
            for (const auto& section : m_outline.sections)
                recovered += section.failures;
            text = tr("✓ PASSED — %1").arg(name);
            if (recovered > 0)
                text += tr(" · %n failed keyword attempt(s) recovered", nullptr, recovered);
            style = "background-color: #e8f5e9; color: #1b5e20;";
            break;
        }
        case analyzer::TestVerdict::Fail:
        {
            if (m_outline.failingStep.empty())
            {
                text = tr("✗ FAILED — %1 — no failing step is recorded in this log "
                          "(the cause may be in the suite setup or in another test's log)").arg(name);
            }
            else
            {
                QString message = QString::fromStdString(m_outline.failureMessage);
                tooltip = message;
                if (message.size() > kMaxBannerMessage)
                    message = message.left(kMaxBannerMessage) + "…";
                text = tr("✗ FAILED — %1 — step \"%2\": %3")
                           .arg(name, QString::fromStdString(m_outline.failingStep), message);
            }
            style = "background-color: #fdecea; color: #b71c1c;";
            break;
        }
        case analyzer::TestVerdict::Unknown:
            text  = tr("No verdict — %1 (the log has no status and ends without TEST_END)").arg(name);
            style = "background-color: #eeeeee; color: #424242;";
            break;
    }
    m_bannerText->setText(text);
    m_bannerText->setToolTip(tooltip);
    m_banner->setStyleSheet(QStringLiteral("QFrame#testStepsBanner { %1 border-radius: 4px; } "
                                           "QLabel { %1 font-weight: bold; }").arg(style));
    m_goToFailure->setVisible(m_outline.decisiveFailureRow.has_value());

    // ── Tree ──────────────────────────────────────────────────────────────
    QTreeWidgetItem* decisiveItem = nullptr;
    for (const auto& section : m_outline.sections)
    {
        QTreeWidgetItem* sectionItem = MakeItem(section);
        m_tree->addTopLevelItem(sectionItem);
        for (const auto& child : section.children)
        {
            QTreeWidgetItem* childItem = MakeItem(child);
            sectionItem->addChild(childItem);
            // The keyword whose span holds the decisive STEP_FAIL.
            const auto& row = m_outline.decisiveFailureRow;
            if (!decisiveItem && row && child.status == analyzer::StepStatus::Failed &&
                *row >= child.firstRow && *row < child.endRow)
                decisiveItem = childItem;
        }
        sectionItem->setExpanded(section.status == analyzer::StepStatus::Failed ||
                                 m_outline.sections.size() == 1);
    }
    if (decisiveItem)
    {
        m_tree->setCurrentItem(decisiveItem);
        m_tree->scrollToItem(decisiveItem);
    }
}

void TestStepsPanel::ActivateItem(QTreeWidgetItem* item)
{
    if (!item)
        return;
    emit NavigateToEvent(static_cast<int>(item->data(kStepColumn, Qt::UserRole).toULongLong()));
}

bool TestStepsPanel::eventFilter(QObject* watched, QEvent* event)
{
    if (watched == m_tree && event->type() == QEvent::KeyPress)
    {
        const int key = static_cast<QKeyEvent*>(event)->key();
        if (key == Qt::Key_Return || key == Qt::Key_Enter)
        {
            ActivateItem(m_tree->currentItem());
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

} // namespace ui::qt
