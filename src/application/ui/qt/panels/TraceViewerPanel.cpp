#include "TraceViewerPanel.hpp"

#include "utils/PanelUtils.hpp"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <set>
#include <string>

namespace ui::qt {

namespace {

/// QTreeWidgetItem that sorts numeric columns (Events, Errors, Duration) correctly.
class TraceTreeItem : public QTreeWidgetItem
{
  public:
    using QTreeWidgetItem::QTreeWidgetItem;

    bool operator<(const QTreeWidgetItem& other) const override
    {
        const int col = treeWidget() ? treeWidget()->sortColumn() : 0;
        // Columns 1 (Events), 2 (Errors), 3 (Duration) are numeric
        if (col >= 1 && col <= 3)
            return data(col, Qt::UserRole).toULongLong() <
                   other.data(col, Qt::UserRole).toULongLong();
        return QTreeWidgetItem::operator<(other);
    }
};

} // anonymous namespace

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

TraceViewerPanel::TraceViewerPanel(db::EventsContainer& events,
                                   EventsTableView*     eventsView,
                                   QWidget*             parent)
    : QWidget(parent), m_events(events), m_eventsView(eventsView)
{
    BuildLayout();
}

void TraceViewerPanel::BuildLayout()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // ── Field selector row ────────────────────────────────────────────────
    auto* ctrlRow = new QHBoxLayout();
    ctrlRow->addWidget(new QLabel(tr("Correlation field:"), this));
    m_fieldCombo = new QComboBox(this);
    m_fieldCombo->setToolTip(
        tr("Field whose value groups related events (e.g. requestId, sessionId, traceId)"));
    m_fieldCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    ctrlRow->addWidget(m_fieldCombo);

    auto* refreshFieldsBtn = new QPushButton(tr("Rescan Fields"), this);
    refreshFieldsBtn->setToolTip(tr("Re-scan available field names from loaded events"));
    ctrlRow->addWidget(refreshFieldsBtn);

    m_clearBtn = new QPushButton(tr("Clear Filter"), this);
    m_clearBtn->setToolTip(tr("Remove the trace filter and show all visible events"));
    m_clearBtn->setEnabled(false);
    ctrlRow->addWidget(m_clearBtn);
    layout->addLayout(ctrlRow);

    // ── Trace tree ────────────────────────────────────────────────────────
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(6);
    m_tree->setHeaderLabels({tr("Trace ID"), tr("Events"), tr("Errors"),
                             tr("Duration"), tr("First Seen"), tr("Last Seen")});
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(4, QHeaderView::Interactive);
    m_tree->header()->setSectionResizeMode(5, QHeaderView::Interactive);
    m_tree->setColumnWidth(4, 160);
    m_tree->setColumnWidth(5, 160);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSortingEnabled(true);
    m_tree->setToolTip(tr("Double-click a row to filter the events view to that trace"));
    layout->addWidget(m_tree, 1);

    // ── Status label ──────────────────────────────────────────────────────
    m_statusLabel = new QLabel(tr("No data"), this);
    m_statusLabel->setAlignment(Qt::AlignRight);
    layout->addWidget(m_statusLabel);

    // ── Connections ───────────────────────────────────────────────────────
    connect(refreshFieldsBtn, &QPushButton::clicked, this, [this]() {
        PopulateFieldCombo();
        const QString field = m_fieldCombo->currentText();
        if (!field.isEmpty())
            RebuildTree(field);
    });

    connect(m_fieldCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, [this](int) {
                const QString field = m_fieldCombo->currentText();
                if (!field.isEmpty())
                    RebuildTree(field);
            });

    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        m_eventsView->ClearFilter();
        m_clearBtn->setEnabled(false);
    });

    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int) {
                if (!item) return;
                const std::string key =
                    item->data(0, Qt::UserRole).toString().toStdString();
                const auto it = m_traceEvents.find(key);
                if (it != m_traceEvents.end() && !it->second.empty())
                {
                    m_eventsView->SetFilteredEvents(it->second);
                    m_clearBtn->setEnabled(true);
                }
            });
}

// ---------------------------------------------------------------------------
// Public slot
// ---------------------------------------------------------------------------

void TraceViewerPanel::Refresh()
{
    if (m_fieldCombo->count() == 0)
        PopulateFieldCombo();

    const QString field = m_fieldCombo->currentText();
    if (field.isEmpty())
    {
        m_tree->clear();
        m_statusLabel->setText(tr("No fields found — load a log file first"));
        return;
    }
    RebuildTree(field);
}

// ---------------------------------------------------------------------------
// Private
// ---------------------------------------------------------------------------

void TraceViewerPanel::PopulateFieldCombo()
{
    const QString saved = m_fieldCombo->currentText();
    m_fieldCombo->blockSignals(true);
    m_fieldCombo->clear();

    std::set<std::string> fieldNames;
    const size_t probe = std::min(m_events.Size(), size_t(200));
    for (size_t i = 0; i < probe; ++i)
        for (const auto& [k, v] : m_events.GetEvent(i).getEventItems())
            fieldNames.insert(k);

    for (const auto& name : fieldNames)
        m_fieldCombo->addItem(QString::fromStdString(name));

    const int idx = m_fieldCombo->findText(saved);
    m_fieldCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_fieldCombo->blockSignals(false);
}

void TraceViewerPanel::RebuildTree(const QString& field)
{
    m_tree->setSortingEnabled(false);
    m_tree->clear();
    m_traceEvents.clear();

    const std::vector<unsigned long> vis = panel_utils::VisibleIndices(m_eventsView, m_events);
    const std::string fieldStr = field.toStdString();

    static const std::vector<std::string> kErrFields{"level", "severity", "type"};
    static const std::vector<std::string> kErrValues{
        "error", "ERROR", "Error", "critical", "CRITICAL", "fatal", "FATAL"};

    struct TraceData
    {
        std::vector<unsigned long> indices;
        QDateTime firstSeen;
        QDateTime lastSeen;
        size_t    errorCount {0};
    };
    std::map<std::string, TraceData> traces;

    for (unsigned long idx : vis)
    {
        const db::LogEvent& logEv = m_events.GetEvent(idx);
        const std::string traceId = logEv.findByKey(fieldStr);
        if (traceId.empty()) continue;

        auto& td = traces[traceId];
        td.indices.push_back(idx);

        for (const auto& tsf : panel_utils::kTsFields)
        {
            const std::string ts = logEv.findByKey(tsf);
            if (ts.empty()) continue;
            const QDateTime dt = panel_utils::ParseTimestamp(QString::fromStdString(ts));
            if (!dt.isValid()) continue;
            if (!td.firstSeen.isValid() || dt < td.firstSeen) td.firstSeen = dt;
            if (!td.lastSeen.isValid()  || dt > td.lastSeen)  td.lastSeen  = dt;
            break;
        }

        for (const auto& errField : kErrFields)
        {
            const std::string val = logEv.findByKey(errField);
            if (val.empty()) continue;
            for (const auto& errVal : kErrValues)
                if (val == errVal) { ++td.errorCount; break; }
            break;
        }
    }

    for (auto& [traceId, td] : traces)
    {
        m_traceEvents[traceId] = td.indices;

        auto* item = new TraceTreeItem(m_tree);
        item->setText(0, QString::fromStdString(traceId));
        item->setData(0, Qt::UserRole, QString::fromStdString(traceId));

        item->setData(1, Qt::UserRole, static_cast<qulonglong>(td.indices.size()));
        item->setText(1, QString::number(td.indices.size()));

        item->setData(2, Qt::UserRole, static_cast<qulonglong>(td.errorCount));
        item->setText(2, QString::number(td.errorCount));
        if (td.errorCount > 0)
            item->setForeground(2, QColor(Qt::red));

        if (td.firstSeen.isValid() && td.lastSeen.isValid())
        {
            const qint64 durMs = td.firstSeen.msecsTo(td.lastSeen);
            QString dur;
            if (durMs < 1000)
                dur = tr("%1 ms").arg(durMs);
            else if (durMs < 60'000)
                dur = tr("%1 s").arg(static_cast<double>(durMs) / 1000.0, 0, 'f', 2);
            else
                dur = tr("%1 min").arg(static_cast<double>(durMs) / 60'000.0, 0, 'f', 1);
            item->setData(3, Qt::UserRole, static_cast<qulonglong>(durMs));
            item->setText(3, dur);
            item->setText(4, td.firstSeen.toString("yyyy-MM-dd HH:mm:ss"));
            item->setText(5, td.lastSeen.toString("yyyy-MM-dd HH:mm:ss"));
        }
    }

    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(1, Qt::DescendingOrder);

    // Count events that actually had the correlation field set
    size_t withValue = 0;
    for (const auto& [id, td] : traces)
        withValue += td.indices.size();

    m_statusLabel->setText(
        tr("%1 unique %2 value(s) across %3 of %4 events")
            .arg(traces.size())
            .arg(field)
            .arg(withValue)
            .arg(vis.size()));
}

} // namespace ui::qt
