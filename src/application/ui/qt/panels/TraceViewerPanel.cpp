#include "TraceViewerPanel.hpp"

#include "Logger.hpp"
#include "utils/PanelUtils.hpp"

#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <set>
#include <string>
#include <unordered_map>

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
        if (col >= 1 && col <= 3)
            return data(col, Qt::UserRole).toULongLong() <
                   other.data(col, Qt::UserRole).toULongLong();
        return QTreeWidgetItem::operator<(other);
    }
};

// Candidate field names for actor/source identification.
static const std::vector<std::string> kActorFields{
    "actor", "source", "component", "service", "module", "sender", "origin"};

// Candidate field names for message/event type.
static const std::vector<std::string> kTypeFields{
    "type", "msgtype", "message_type", "event_type", "action", "level", "severity"};

// Candidate field names for parent-span linking.
static const std::vector<std::string> kParentFields{
    "parentId", "parentSpanId", "parent_id", "parent_span_id", "parent"};

// Candidate field names for span ID.
static const std::vector<std::string> kSpanFields{
    "spanId", "span_id", "spanid"};

static std::string FirstFieldValue(const db::LogEvent& ev,
                                   const std::vector<std::string>& candidates)
{
    for (const auto& f : candidates)
    {
        const std::string v = ev.findByKey(f);
        if (!v.empty()) return v;
    }
    return {};
}

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

    // ── Search bar ────────────────────────────────────────────────────────
    m_searchEdit = new QLineEdit(this);
    m_searchEdit->setPlaceholderText(tr("Filter traces…"));
    m_searchEdit->setClearButtonEnabled(true);
    layout->addWidget(m_searchEdit);

    // ── Trace tree ────────────────────────────────────────────────────────
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(6);
    m_tree->setHeaderLabels({tr("Trace / Span"), tr("Events"), tr("Errors"),
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
    m_tree->setToolTip(tr("Double-click a row to filter the events view to that trace.\n"
                          "Child rows show per-actor and per-type breakdowns."));
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

    connect(m_searchEdit, &QLineEdit::textChanged,
            this, &TraceViewerPanel::FilterTree);

    connect(m_tree, &QTreeWidget::itemDoubleClicked,
            this, [this](QTreeWidgetItem* item, int) {
                if (!item) return;
                const std::string key =
                    item->data(0, Qt::UserRole).toString().toStdString();
                const auto it = m_traceEvents.find(key);
                if (it != m_traceEvents.end() && !it->second.empty())
                {
                    util::Logger::Debug("[TraceViewer] Filtering to trace '{}' ({} events)",
                        key, it->second.size());
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
        util::Logger::Warn("[TraceViewer] Refresh called but no correlation fields available");
        m_tree->clear();
        m_statusLabel->setText(tr("No fields found — load a log file first"));
        return;
    }
    util::Logger::Debug("[TraceViewer] Refreshing trace tree with field '{}'",
                        field.toStdString());
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
    const size_t probe = (std::min)(m_events.Size(), size_t(1000));
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
        QDateTime  firstSeen;
        QDateTime  lastSeen;
        size_t     errorCount {0};
        // actor name → (eventCount, errorCount)
        std::unordered_map<std::string, std::pair<size_t,size_t>> actors;
        // message type → count
        std::unordered_map<std::string, size_t> msgTypes;
        // spanId → parentId (for call-tree building; populated when present)
        std::unordered_map<std::string, std::string> spanParent;
    };
    std::map<std::string, TraceData> traces;

    for (unsigned long idx : vis)
    {
        const db::LogEvent& logEv = m_events.GetEvent(idx);
        const std::string traceId = logEv.findByKey(fieldStr);
        if (traceId.empty()) continue;

        auto& td = traces[traceId];
        td.indices.push_back(idx);

        // Timestamp
        for (const auto& tsf : panel_utils::kTsFields)
        {
            const std::string ts = logEv.findByKey(tsf);
            if (ts.empty()) continue;
            const QDateTime dt = panel_utils::ParseTimestamp(
                QString::fromStdString(ts));
            if (!dt.isValid()) continue;
            if (!td.firstSeen.isValid() || dt < td.firstSeen) td.firstSeen = dt;
            if (!td.lastSeen.isValid()  || dt > td.lastSeen)  td.lastSeen  = dt;
            break;
        }

        // Error detection
        bool isError = false;
        for (const auto& errField : kErrFields)
        {
            const std::string val = logEv.findByKey(errField);
            if (val.empty()) continue;
            for (const auto& errVal : kErrValues)
                if (val == errVal) { ++td.errorCount; isError = true; break; }
            break;
        }

        // Actor breakdown
        const std::string actor = FirstFieldValue(logEv, kActorFields);
        if (!actor.empty())
        {
            auto& [cnt, errs] = td.actors[actor];
            ++cnt;
            if (isError) ++errs;
        }

        // Message-type breakdown
        const std::string msgType = FirstFieldValue(logEv, kTypeFields);
        if (!msgType.empty())
            ++td.msgTypes[msgType];

        // Span hierarchy
        const std::string spanId   = FirstFieldValue(logEv, kSpanFields);
        const std::string parentId = FirstFieldValue(logEv, kParentFields);
        if (!spanId.empty())
            td.spanParent.emplace(spanId, parentId);
    }

    // Build span → QTreeWidgetItem* lookup for call-tree parenting.
    for (auto& [traceId, td] : traces)
    {
        m_traceEvents[traceId] = td.indices;

        const bool hasErrors  = td.errorCount > 0;
        const bool hasActors  = !td.actors.empty();
        const bool hasSpans   = !td.spanParent.empty();
        const bool hasTypes   = !td.msgTypes.empty();

        // ── Top-level trace item ──────────────────────────────────────────
        auto* item = new TraceTreeItem(m_tree);
        item->setText(0, QString::fromStdString(traceId));
        item->setData(0, Qt::UserRole, QString::fromStdString(traceId));

        if (hasErrors)
            item->setForeground(0, QColor(Qt::red));

        item->setData(1, Qt::UserRole, static_cast<qulonglong>(td.indices.size()));
        item->setText(1, QString::number(td.indices.size()));

        item->setData(2, Qt::UserRole, static_cast<qulonglong>(td.errorCount));
        item->setText(2, QString::number(td.errorCount));
        if (hasErrors)
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

        // ── Span call-tree children ───────────────────────────────────────
        if (hasSpans)
        {
            // Build span-item map; spans without a parent are direct children
            // of the trace item; others are children of their parent span item.
            std::unordered_map<std::string, QTreeWidgetItem*> spanItems;
            // Two-pass: first create all span items, then parent them.
            for (const auto& [spanId, parentId] : td.spanParent)
            {
                auto* spanItem = new TraceTreeItem();
                spanItem->setText(0, QString::fromStdString(
                    "  span: " + spanId +
                    (parentId.empty() ? "" : "  ← " + parentId)));
                spanItems[spanId] = spanItem;
            }
            for (const auto& [spanId, parentId] : td.spanParent)
            {
                auto* spanItem = spanItems[spanId];
                if (!parentId.empty())
                {
                    const auto pit = spanItems.find(parentId);
                    if (pit != spanItems.end())
                        pit->second->addChild(spanItem);
                    else
                        item->addChild(spanItem);
                }
                else
                {
                    item->addChild(spanItem);
                }
            }
        }

        // ── Actor breakdown children ──────────────────────────────────────
        if (hasActors)
        {
            auto* actorHeader = new QTreeWidgetItem(item);
            actorHeader->setText(0, tr("  Actors"));
            actorHeader->setForeground(0, QColor(Qt::gray));

            // Sort actors by event count descending.
            std::vector<std::pair<size_t, std::string>> sorted;
            sorted.reserve(td.actors.size());
            for (const auto& [name, cnts] : td.actors)
                sorted.emplace_back(cnts.first, name);
            std::sort(sorted.rbegin(), sorted.rend());

            for (const auto& [cnt, name] : sorted)
            {
                const auto& [evCnt, errCnt] = td.actors[name];
                auto* a = new QTreeWidgetItem(actorHeader);
                a->setText(0, QString::fromStdString("    " + name));
                a->setData(1, Qt::UserRole, static_cast<qulonglong>(evCnt));
                a->setText(1, QString::number(evCnt));
                if (errCnt > 0)
                {
                    a->setData(2, Qt::UserRole, static_cast<qulonglong>(errCnt));
                    a->setText(2, QString::number(errCnt));
                    a->setForeground(2, QColor(Qt::red));
                }
            }
        }

        // ── Message-type breakdown children ──────────────────────────────
        if (hasTypes)
        {
            auto* typeHeader = new QTreeWidgetItem(item);
            typeHeader->setText(0, tr("  Message Types"));
            typeHeader->setForeground(0, QColor(Qt::gray));

            std::vector<std::pair<size_t, std::string>> sorted;
            sorted.reserve(td.msgTypes.size());
            for (const auto& [t, cnt] : td.msgTypes)
                sorted.emplace_back(cnt, t);
            std::sort(sorted.rbegin(), sorted.rend());

            for (const auto& [cnt, type] : sorted)
            {
                auto* t = new QTreeWidgetItem(typeHeader);
                t->setText(0, QString::fromStdString("    " + type));
                t->setData(1, Qt::UserRole, static_cast<qulonglong>(cnt));
                t->setText(1, QString::number(cnt));
            }
        }
    }

    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(1, Qt::DescendingOrder);

    // Apply pending search filter.
    const QString search = m_searchEdit->text();
    if (!search.isEmpty())
        FilterTree(search);

    size_t withValue = 0;
    for (const auto& [id, td] : traces)
        withValue += td.indices.size();

    util::Logger::Info("[TraceViewer] Rebuilt tree: {} unique '{}' value(s) across {} of {} events",
        traces.size(), field.toStdString(), withValue, vis.size());

    m_statusLabel->setText(
        tr("%1 unique %2 value(s) across %3 of %4 events")
            .arg(traces.size())
            .arg(field)
            .arg(withValue)
            .arg(vis.size()));
}

void TraceViewerPanel::FilterTree(const QString& text)
{
    const bool showAll = text.trimmed().isEmpty();
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
    {
        QTreeWidgetItem* item = m_tree->topLevelItem(i);
        const bool match = showAll ||
            item->text(0).contains(text, Qt::CaseInsensitive);
        item->setHidden(!match);
    }
}

} // namespace ui::qt
