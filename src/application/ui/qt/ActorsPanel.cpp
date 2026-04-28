#include "ActorsPanel.hpp"

#include "ActorDefinition.hpp"
#include "EventsContainer.hpp"
#include "EventsTableView.hpp"

#include <QGroupBox>
#include <QHeaderView>
#include <QLabel>
#include <QRegularExpression>
#include <QVBoxLayout>

#include <algorithm>

namespace ui::qt {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ActorsPanel::ActorsPanel(db::EventsContainer& events,
                         EventsTableView*     eventsView,
                         QWidget*             parent)
    : QWidget(parent), m_events(events), m_eventsView(eventsView)
{
    BuildLayout();
}

void ActorsPanel::BuildLayout()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // ── Actors table ──────────────────────────────────────────────────────
    m_table = new QTableWidget(0, 6, this);
    m_table->setHorizontalHeaderLabels(
        {tr("Actor"), tr("Events"), tr("Errors"), tr("Types"),
         tr("First Seen"), tr("Last Seen")});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Interactive);
    m_table->setColumnWidth(4, 160);
    m_table->setColumnWidth(5, 160);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->hide();
    m_table->setAlternatingRowColors(true);
    m_table->setSortingEnabled(true);
    layout->addWidget(m_table, 1);

    // ── Status label ──────────────────────────────────────────────────────
    m_statusLabel = new QLabel(tr("No data"), this);
    m_statusLabel->setAlignment(Qt::AlignRight);
    layout->addWidget(m_statusLabel);

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) {
        auto* item = m_table->item(row, 0);
        if (!item) return;
        FilterByActor(item->text().toStdString());
    });
}

// ---------------------------------------------------------------------------
// Public slots
// ---------------------------------------------------------------------------

void ActorsPanel::Refresh()
{
    const auto vis = VisibleIndices();
    m_actorCache.clear();

    const bool hasDefinitions = !m_definitions.empty() &&
        std::any_of(m_definitions.begin(), m_definitions.end(),
                    [](const ActorDefinition& d) { return d.enabled; });

    if (hasDefinitions)
    {
        RefreshWithDefinitions(vis);
    }
    else
    {
        // No definitions — leave table empty
        m_statusLabel->setText(tr("Define actors in the \"Actor Definitions\" panel"));
    }
}

void ActorsPanel::SetDefinitions(const std::vector<ActorDefinition>& defs)
{
    m_definitions = defs;
    Refresh();
}

// ---------------------------------------------------------------------------
// Private: two refresh modes
// ---------------------------------------------------------------------------

namespace {
// Helper used by both modes to accumulate error/type/timestamp stats into ActorData
void AccumulateEventStats(
    ActorsPanel::ActorData&    data,
    const db::LogEvent&        ev,
    unsigned long              idx)
{
    data.indices.push_back(idx);

    static const std::vector<std::string> kTsFields {
        "timestamp", "time", "datetime", "@timestamp", "date"};
    static const std::vector<std::string> kErrFields {"level", "severity", "type"};
    static const std::vector<std::string> kErrValues {
        "error", "ERROR", "Error", "critical", "CRITICAL", "fatal", "FATAL"};

    for (const auto& f : kTsFields)
    {
        const std::string ts = ev.findByKey(f);
        if (!ts.empty())
        {
            if (data.firstSeen.empty() || ts < data.firstSeen) data.firstSeen = ts;
            if (data.lastSeen.empty()  || ts > data.lastSeen)  data.lastSeen  = ts;
            break;
        }
    }

    for (const auto& errField : kErrFields)
    {
        const std::string val = ev.findByKey(errField);
        if (!val.empty())
        {
            for (const auto& errVal : kErrValues)
                if (val == errVal) { ++data.errorCount; break; }
            data.types.insert(val);
            break;
        }
    }
}
} // anonymous namespace

void ActorsPanel::RefreshWithDefinitions(const std::vector<unsigned long>& vis)
{
    // Pre-compile regexps for enabled definitions
    struct CompiledDef
    {
        std::string           name;
        std::string           field; // empty = any field
        QRegularExpression    re;
        bool                  useCaptures {false};
    };
    std::vector<CompiledDef> compiled;
    for (const auto& def : m_definitions)
    {
        if (!def.enabled || def.pattern.empty()) continue;
        QRegularExpression re(QString::fromStdString(def.pattern));
        if (!re.isValid()) continue;
        compiled.push_back({def.name, def.field, std::move(re), def.useCaptures});
    }

    for (unsigned long idx : vis)
    {
        const db::LogEvent& ev = m_events.GetEvent(static_cast<int>(idx));

        for (const auto& cdef : compiled)
        {
            auto processMatch = [&](const QRegularExpressionMatch& m) {
                if (!m.hasMatch()) return false;
                if (cdef.useCaptures)
                {
                    // Each non-empty capture group becomes a distinct actor
                    bool anyCapture = false;
                    for (int g = 1; g <= cdef.re.captureCount(); ++g)
                    {
                        const QString captured = m.captured(g);
                        if (captured.isEmpty()) continue;
                        AccumulateEventStats(
                            m_actorCache[captured.toStdString()], ev, idx);
                        anyCapture = true;
                    }
                    return anyCapture;
                }
                // Fixed actor name mode
                AccumulateEventStats(m_actorCache[cdef.name], ev, idx);
                return true;
            };

            bool matched = false;
            if (cdef.field.empty())
            {
                for (const auto& [key, val] : ev.getEventItems())
                {
                    if (processMatch(cdef.re.match(QString::fromStdString(val))))
                    {
                        matched = true;
                        break;
                    }
                }
            }
            else
            {
                const std::string val = ev.findByKey(cdef.field);
                matched = processMatch(cdef.re.match(QString::fromStdString(val)));
            }

            if (matched)
                break; // first matching definition wins
        }
    }

    PopulateActorTable(vis.size());
}

void ActorsPanel::PopulateActorTable(size_t totalVisible)
{
    auto makeItem = [](const QString& text,
                       Qt::Alignment  align = Qt::AlignLeft | Qt::AlignVCenter) {
        auto* it = new QTableWidgetItem(text);
        it->setTextAlignment(align);
        return it;
    };

    m_table->setSortingEnabled(false);
    m_table->setRowCount(0);

    for (const auto& [actor, data] : m_actorCache)
    {
        const int row = m_table->rowCount();
        m_table->insertRow(row);

        m_table->setItem(row, 0, makeItem(QString::fromStdString(actor)));

        auto* countItem = makeItem(QString::number(data.indices.size()),
                                   Qt::AlignRight | Qt::AlignVCenter);
        countItem->setData(Qt::UserRole, static_cast<qulonglong>(data.indices.size()));
        m_table->setItem(row, 1, countItem);

        auto* errItem = makeItem(QString::number(data.errorCount),
                                 Qt::AlignRight | Qt::AlignVCenter);
        errItem->setData(Qt::UserRole, static_cast<qulonglong>(data.errorCount));
        if (data.errorCount > 0)
            errItem->setForeground(QColor(Qt::red));
        m_table->setItem(row, 2, errItem);

        QStringList types;
        for (const auto& t : data.types) types << QString::fromStdString(t);
        m_table->setItem(row, 3, makeItem(types.join(", ")));

        m_table->setItem(row, 4, makeItem(QString::fromStdString(data.firstSeen)));
        m_table->setItem(row, 5, makeItem(QString::fromStdString(data.lastSeen)));
    }

    m_table->setSortingEnabled(true);
    m_table->sortByColumn(1, Qt::DescendingOrder);

    m_statusLabel->setText(
        tr("%1 actor(s) in %2 event(s)").arg(m_actorCache.size()).arg(totalVisible));
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void ActorsPanel::FilterByActor(const std::string& actorValue)
{
    if (!m_eventsView) return;

    const auto it = m_actorCache.find(actorValue);
    if (it == m_actorCache.end()) return;

    m_eventsView->SetFilteredEvents(it->second.indices);
}

std::vector<unsigned long> ActorsPanel::VisibleIndices() const
{
    const std::vector<unsigned long>* filtered = m_eventsView->GetFilteredIndices();
    if (filtered && !filtered->empty())
        return *filtered;

    const size_t total = m_events.Size();
    std::vector<unsigned long> all;
    all.reserve(total);
    for (size_t i = 0; i < total; ++i)
        all.push_back(static_cast<unsigned long>(i));
    return all;
}

} // namespace ui::qt
