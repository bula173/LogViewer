#include "ActorsPanel.hpp"

#include "ActorDefinition.hpp"
#include "Config.hpp"
#include "EventsContainer.hpp"
#include "EventsTableView.hpp"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QRegularExpression>
#include <QSignalBlocker>
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

    // ── Field selector ────────────────────────────────────────────────────
    // Mode label (shown above the field row)
    m_modeLabel = new QLabel(tr("Mode: auto-detect"), this);
    m_modeLabel->setStyleSheet("color: gray; font-style: italic;");
    layout->addWidget(m_modeLabel);

    auto* fieldRow = new QHBoxLayout();
    fieldRow->addWidget(new QLabel(tr("Actor field:"), this));
    m_fieldCombo = new QComboBox(this);
    m_fieldCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_fieldCombo->setToolTip(tr("Select the log field that identifies individual actors"));
    fieldRow->addWidget(m_fieldCombo);
    layout->addLayout(fieldRow);

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

    // ── Action buttons ────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    m_clearBtn = new QPushButton(tr("Clear Filter"), this);
    m_clearBtn->setEnabled(false);
    btnRow->addStretch();
    btnRow->addWidget(m_clearBtn);
    layout->addLayout(btnRow);

    // ── Status label ──────────────────────────────────────────────────────
    m_statusLabel = new QLabel(tr("No data"), this);
    m_statusLabel->setAlignment(Qt::AlignRight);
    layout->addWidget(m_statusLabel);

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_fieldCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        // Persist the chosen field
        config::GetConfig().actorField = m_fieldCombo->currentText().toStdString();
        config::GetConfig().SaveConfig();
        Refresh();
    });

    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) {
        auto* item = m_table->item(row, 0);
        if (!item) return;
        FilterByActor(item->text().toStdString());
        m_clearBtn->setEnabled(true);
    });

    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        if (m_eventsView)
            m_eventsView->SetFilteredEvents({});
        m_clearBtn->setEnabled(false);
        m_table->clearSelection();
    });
}

// ---------------------------------------------------------------------------
// Public slots
// ---------------------------------------------------------------------------

void ActorsPanel::Refresh()
{
    PopulateFieldCombo();
    const auto vis = VisibleIndices();
    m_actorCache.clear();

    const bool hasDefinitions = !m_definitions.empty() &&
        std::any_of(m_definitions.begin(), m_definitions.end(),
                    [](const ActorDefinition& d) { return d.enabled; });

    if (hasDefinitions)
    {
        m_modeLabel->setText(tr("Mode: regexp definitions (%1 active)")
            .arg(std::count_if(m_definitions.begin(), m_definitions.end(),
                               [](const ActorDefinition& d){ return d.enabled; })));
        m_modeLabel->setStyleSheet("color: #0070c0; font-style: italic;");
        m_fieldCombo->setEnabled(false);
        RefreshWithDefinitions(vis);
    }
    else
    {
        m_modeLabel->setText(tr("Mode: auto-detect by field"));
        m_modeLabel->setStyleSheet("color: gray; font-style: italic;");
        m_fieldCombo->setEnabled(true);
        RefreshAutoDetect(vis);
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
    };
    std::vector<CompiledDef> compiled;
    for (const auto& def : m_definitions)
    {
        if (!def.enabled || def.pattern.empty()) continue;
        QRegularExpression re(QString::fromStdString(def.pattern));
        if (!re.isValid()) continue;
        compiled.push_back({def.name, def.field, std::move(re)});
    }

    for (unsigned long idx : vis)
    {
        const db::LogEvent& ev = m_events.GetEvent(static_cast<int>(idx));

        for (const auto& cdef : compiled)
        {
            bool matched = false;
            if (cdef.field.empty())
            {
                // Match against all field values
                for (const auto& [key, val] : ev.getEventItems())
                {
                    if (cdef.re.match(QString::fromStdString(val)).hasMatch())
                    {
                        matched = true;
                        break;
                    }
                }
            }
            else
            {
                const std::string val = ev.findByKey(cdef.field);
                matched = cdef.re.match(QString::fromStdString(val)).hasMatch();
            }

            if (matched)
            {
                AccumulateEventStats(m_actorCache[cdef.name], ev, idx);
                break; // first matching definition wins
            }
        }
    }

    PopulateActorTable(vis.size());
}

void ActorsPanel::RefreshAutoDetect(const std::vector<unsigned long>& vis)
{
    const std::string field = m_fieldCombo->currentText().toStdString();

    for (unsigned long idx : vis)
    {
        const db::LogEvent& ev    = m_events.GetEvent(static_cast<int>(idx));
        const std::string   actor = ev.findByKey(field);
        if (actor.empty()) continue;
        AccumulateEventStats(m_actorCache[actor], ev, idx);
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

void ActorsPanel::PopulateFieldCombo()
{
    auto* model = m_eventsView ? m_eventsView->model() : nullptr;
    if (!model) return;

    const QString preferred =
        QString::fromStdString(config::GetConfig().actorField);

    const QSignalBlocker blocker(m_fieldCombo);
    m_fieldCombo->clear();

    const int cols = model->columnCount();
    for (int c = 0; c < cols; ++c)
    {
        m_fieldCombo->addItem(
            model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString());
    }

    // Select the persisted field (or default to first column)
    const int idx = m_fieldCombo->findText(preferred);
    m_fieldCombo->setCurrentIndex(idx >= 0 ? idx : 0);
}

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
