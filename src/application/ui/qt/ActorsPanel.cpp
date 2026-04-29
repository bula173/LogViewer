#include "ActorsPanel.hpp"

#include "ActorDefinition.hpp"
#include "EventsContainer.hpp"
#include "EventsTableView.hpp"

#include <QClipboard>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFontDatabase>
#include <QFormLayout>
#include <QGuiApplication>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QSpinBox>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>
#include <sstream>

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

    // ── Actors tree ───────────────────────────────────────────────────────
    m_tree = new QTreeWidget(this);
    m_tree->setColumnCount(6);
    m_tree->setHeaderLabels(
        {tr("Actor"), tr("Events"), tr("Errors"), tr("Types"),
         tr("First Seen"), tr("Last Seen")});
    m_tree->header()->setStretchLastSection(false);
    m_tree->header()->setSectionResizeMode(0, QHeaderView::Stretch);
    m_tree->header()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
    m_tree->header()->setSectionResizeMode(4, QHeaderView::Interactive);
    m_tree->header()->setSectionResizeMode(5, QHeaderView::Interactive);
    m_tree->setColumnWidth(4, 160);
    m_tree->setColumnWidth(5, 160);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_tree->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_tree->setAlternatingRowColors(true);
    m_tree->setSortingEnabled(true);
    layout->addWidget(m_tree, 1);

    // ── Status label ──────────────────────────────────────────────────────
    m_statusLabel = new QLabel(tr("No data"), this);
    m_statusLabel->setAlignment(Qt::AlignRight);
    layout->addWidget(m_statusLabel);

    // ── Sequence diagram button ───────────────────────────────────────────
    m_seqDiagBtn = new QPushButton(tr("Sequence Diagram…"), this);
    m_seqDiagBtn->setToolTip(tr(
        "Generate a PlantUML sequence diagram from the selected actors.\n"
        "Select one or more actors in the tree above, then click this button.\n"
        "If nothing is selected all actors are used."));
    layout->addWidget(m_seqDiagBtn);

    // ── Connections ───────────────────────────────────────────────────────
    // Debounce: QTreeWidget::itemChanged fires multiple times when tri-state
    // propagation updates all children. QTimer coalesces them into one call.
    connect(m_tree, &QTreeWidget::itemChanged, this,
            [this](QTreeWidgetItem*, int col) {
                if (col != 0) return;
                QTimer::singleShot(0, this, &ActorsPanel::ApplyCheckedFilter);
            });

    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &ActorsPanel::ShowActorContextMenu);

    connect(m_seqDiagBtn, &QPushButton::clicked,
            this, &ActorsPanel::ShowSequenceDiagram);
}

// ---------------------------------------------------------------------------
// Public slots
// ---------------------------------------------------------------------------

void ActorsPanel::Refresh()
{
    // If we triggered this reset ourselves (actor checkbox filter), don't rebuild
    // the tree — the cache and check states are still valid.
    if (m_ignoreNextRefresh)
    {
        m_ignoreNextRefresh = false;
        return;
    }

    const auto vis = VisibleIndices();

    // ── Snapshot which actors are currently unchecked so we can restore
    //    after rebuilding the tree (filter changes cause model reset → Refresh).
    m_uncheckedActors.clear();
    for (int g = 0; g < m_tree->topLevelItemCount(); ++g)
    {
        QTreeWidgetItem* top = m_tree->topLevelItem(g);
        if (top->childCount() == 0)
        {
            if (top->checkState(0) != Qt::Checked)
            {
                const std::string key =
                    top->data(0, Qt::UserRole).toString().toStdString() + '\0' +
                    top->data(0, Qt::UserRole + 1).toString().toStdString();
                m_uncheckedActors.insert(key);
            }
        }
        else
        {
            for (int c = 0; c < top->childCount(); ++c)
            {
                QTreeWidgetItem* ch = top->child(c);
                if (ch->checkState(0) != Qt::Checked)
                {
                    const std::string key =
                        ch->data(0, Qt::UserRole).toString().toStdString() + '\0' +
                        ch->data(0, Qt::UserRole + 1).toString().toStdString();
                    m_uncheckedActors.insert(key);
                }
            }
        }
    }

    m_groupedCache.clear();

    const bool hasDefinitions = !m_definitions.empty() &&
        std::any_of(m_definitions.begin(), m_definitions.end(),
                    [](const ActorDefinition& d) { return d.enabled; });

    if (hasDefinitions)
    {
        RefreshWithDefinitions(vis);
        // Re-apply actor filter if any actors are unchecked so that external
        // filter changes (e.g. clicking "Apply Filter" for type filters) do not
        // silently clear the actor-based filter.
        if (!m_uncheckedActors.empty())
            QTimer::singleShot(0, this, &ActorsPanel::ApplyCheckedFilter);
    }
    else
    {
        QSignalBlocker blocker(m_tree);
        m_tree->clear();
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

/// QTreeWidgetItem subclass that sorts numeric columns (Events, Errors) correctly.
class ActorTreeItem : public QTreeWidgetItem
{
public:
    using QTreeWidgetItem::QTreeWidgetItem;

    bool operator<(const QTreeWidgetItem& other) const override
    {
        const int col = treeWidget() ? treeWidget()->sortColumn() : 0;
        if (col == 1 || col == 2) // numeric: Events, Errors
        {
            return data(col, Qt::UserRole).toULongLong() <
                   other.data(col, Qt::UserRole).toULongLong();
        }
        return QTreeWidgetItem::operator<(other);
    }
};

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
        std::string        name;
        std::string        field; // empty = any field
        QRegularExpression re;
        bool               useCaptures {false};
    };
    std::vector<CompiledDef> compiled;
    for (const auto& def : m_definitions)
    {
        if (!def.enabled || def.pattern.empty()) continue;
        QRegularExpression re(QString::fromStdString(def.pattern));
        if (!re.isValid()) continue;
        compiled.push_back({def.name, def.field, std::move(re), def.useCaptures});
    }

    // Pre-create group entries so the useCaptures flag is set even for empty groups
    for (const auto& cdef : compiled)
        m_groupedCache[cdef.name].useCaptures = cdef.useCaptures;

    for (unsigned long idx : vis)
    {
        const db::LogEvent& ev = m_events.GetEvent(static_cast<int>(idx));

        for (const auto& cdef : compiled)
        {
            auto processMatch = [&](const QRegularExpressionMatch& m) -> bool {
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
                            m_groupedCache[cdef.name].actors[captured.toStdString()],
                            ev, idx);
                        anyCapture = true;
                    }
                    return anyCapture;
                }
                // Fixed actor name mode
                AccumulateEventStats(
                    m_groupedCache[cdef.name].actors[cdef.name], ev, idx);
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

    PopulateActorTree(vis.size());
}

void ActorsPanel::PopulateActorTree(size_t totalVisible)
{
    QSignalBlocker blocker(m_tree);
    m_tree->setSortingEnabled(false);
    m_tree->clear();

    // Build fast lookup from definition name → definition pointer
    std::map<std::string, const ActorDefinition*> defByName;
    for (const auto& def : m_definitions)
        defByName[def.name] = &def;

    // Helper: format actor display name with optional "→ target" suffix.
    // For subactors (children of a capture-group), subActorDirectedTo is
    // checked first, then the definition-level directedTo as a fallback.
    auto makeActorLabel = [&](const std::string& actorName,
                               const std::string& dName,
                               bool               isSubActor) -> QString {
        std::string target;
        const auto dit = defByName.find(dName);
        if (dit != defByName.end())
        {
            const ActorDefinition* def = dit->second;
            if (isSubActor)
            {
                const auto sit = def->subActorDirectedTo.find(actorName);
                target = (sit != def->subActorDirectedTo.end())
                             ? sit->second
                             : def->directedTo; // fallback
            }
            else
            {
                target = def->directedTo;
            }
        }
        if (!target.empty())
            return QString("%1  \u2192 %2")
                .arg(QString::fromStdString(actorName),
                     QString::fromStdString(target));
        return QString::fromStdString(actorName);
    };

    size_t totalActors = 0;

    for (const auto& [defName, group] : m_groupedCache)
    {
        if (group.actors.empty()) continue;
        totalActors += group.actors.size();

        if (!group.useCaptures)
        {
            // Fixed-name definition → single flat top-level item with checkbox
            const auto& [actorName, data] = *group.actors.begin();

            auto* item = new ActorTreeItem(m_tree);
            item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
            item->setCheckState(0, Qt::Checked);
            item->setText(0, makeActorLabel(actorName, defName, false));
            item->setData(1, Qt::UserRole, static_cast<qulonglong>(data.indices.size()));
            item->setText(1, QString::number(data.indices.size()));
            item->setData(2, Qt::UserRole, static_cast<qulonglong>(data.errorCount));
            item->setText(2, QString::number(data.errorCount));
            if (data.errorCount > 0)
                item->setForeground(2, QColor(Qt::red));
            QStringList types;
            for (const auto& t : data.types) types << QString::fromStdString(t);
            item->setText(3, types.join(", "));
            item->setText(4, QString::fromStdString(data.firstSeen));
            item->setText(5, QString::fromStdString(data.lastSeen));
            item->setData(0, Qt::UserRole,     QString::fromStdString(defName));
            item->setData(0, Qt::UserRole + 1, QString::fromStdString(actorName));
            // Restore previous check state
            const std::string key = defName + '\0' + actorName;
            if (m_uncheckedActors.count(key))
                item->setCheckState(0, Qt::Unchecked);
        }
        else
        {
            // Capture-group definition → bold group header + individual child items
            size_t totalEvents = 0;
            size_t totalErrors = 0;
            for (const auto& [a, d] : group.actors)
            {
                totalEvents += d.indices.size();
                totalErrors += d.errorCount;
            }

            auto* groupItem = new ActorTreeItem(m_tree);
            groupItem->setFlags(groupItem->flags() |
                                Qt::ItemIsAutoTristate | Qt::ItemIsUserCheckable);
            groupItem->setCheckState(0, Qt::Checked);
            QFont font = groupItem->font(0);
            font.setBold(true);
            groupItem->setFont(0, font);
            groupItem->setText(
                0, tr("%1  (capture group)").arg(QString::fromStdString(defName)));
            groupItem->setData(1, Qt::UserRole, static_cast<qulonglong>(totalEvents));
            groupItem->setText(1, QString::number(totalEvents));
            groupItem->setData(2, Qt::UserRole, static_cast<qulonglong>(totalErrors));
            groupItem->setText(2, QString::number(totalErrors));
            if (totalErrors > 0)
                groupItem->setForeground(2, QColor(Qt::red));

            for (const auto& [actorName, data] : group.actors)
            {
                auto* child = new ActorTreeItem(groupItem);
                child->setFlags(child->flags() | Qt::ItemIsUserCheckable);
                child->setCheckState(0, Qt::Checked);
                child->setText(0, makeActorLabel(actorName, defName, true));
                child->setData(1, Qt::UserRole, static_cast<qulonglong>(data.indices.size()));
                child->setText(1, QString::number(data.indices.size()));
                child->setData(2, Qt::UserRole, static_cast<qulonglong>(data.errorCount));
                child->setText(2, QString::number(data.errorCount));
                if (data.errorCount > 0)
                    child->setForeground(2, QColor(Qt::red));
                QStringList types;
                for (const auto& t : data.types) types << QString::fromStdString(t);
                child->setText(3, types.join(", "));
                child->setText(4, QString::fromStdString(data.firstSeen));
                child->setText(5, QString::fromStdString(data.lastSeen));
                child->setData(0, Qt::UserRole,     QString::fromStdString(defName));
                child->setData(0, Qt::UserRole + 1, QString::fromStdString(actorName));
                // Restore previous check state
                const std::string key = defName + '\0' + actorName;
                if (m_uncheckedActors.count(key))
                    child->setCheckState(0, Qt::Unchecked);
            }
            groupItem->setExpanded(true);
        }
    }

    m_tree->setSortingEnabled(true);
    m_tree->sortByColumn(1, Qt::DescendingOrder);

    m_statusLabel->setText(
        tr("%1 actor(s) in %2 event(s)").arg(totalActors).arg(totalVisible));
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void ActorsPanel::ApplyCheckedFilter()
{
    if (!m_eventsView) return;

    // Keep m_uncheckedActors in sync so Refresh() can restore check states
    m_uncheckedActors.clear();

    std::vector<unsigned long> combined;

    for (int g = 0; g < m_tree->topLevelItemCount(); ++g)
    {
        QTreeWidgetItem* topItem = m_tree->topLevelItem(g);

        if (topItem->childCount() == 0)
        {
            // Flat fixed-name item
            const std::string defName =
                topItem->data(0, Qt::UserRole).toString().toStdString();
            const std::string actorName =
                topItem->data(0, Qt::UserRole + 1).toString().toStdString();
            if (topItem->checkState(0) == Qt::Checked)
            {
                const auto git = m_groupedCache.find(defName);
                if (git != m_groupedCache.end())
                {
                    const auto ait = git->second.actors.find(actorName);
                    if (ait != git->second.actors.end())
                        for (unsigned long idx : ait->second.indices)
                            combined.push_back(idx);
                }
            }
            else
            {
                m_uncheckedActors.insert(defName + '\0' + actorName);
            }
        }
        else
        {
            // Capture-group: check each child independently
            for (int c = 0; c < topItem->childCount(); ++c)
            {
                QTreeWidgetItem* child = topItem->child(c);
                const std::string defName =
                    child->data(0, Qt::UserRole).toString().toStdString();
                const std::string actorName =
                    child->data(0, Qt::UserRole + 1).toString().toStdString();
                if (child->checkState(0) != Qt::Checked)
                {
                    m_uncheckedActors.insert(defName + '\0' + actorName);
                    continue;
                }
                const auto git = m_groupedCache.find(defName);
                if (git != m_groupedCache.end())
                {
                    const auto ait = git->second.actors.find(actorName);
                    if (ait != git->second.actors.end())
                        for (unsigned long idx : ait->second.indices)
                            combined.push_back(idx);
                }
            }
        }
    }

    if (combined.empty())
    {
        m_ignoreNextRefresh = true;
        m_eventsView->ClearFilter();
        return;
    }

    std::sort(combined.begin(), combined.end());
    combined.erase(std::unique(combined.begin(), combined.end()), combined.end());

    // If every actor is checked the union equals the base set — just clear the filter.
    if (m_uncheckedActors.empty())
    {
        m_ignoreNextRefresh = true;
        m_eventsView->ClearFilter();
        return;
    }

    m_ignoreNextRefresh = true;
    m_eventsView->SetFilteredEvents(combined);
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

// ---------------------------------------------------------------------------
// Actor context menu — set / clear directed-to
// ---------------------------------------------------------------------------

void ActorsPanel::ShowActorContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    if (!item) return;

    // Group headers (non-leaf items) don't represent a single actor
    if (item->childCount() > 0) return;

    const std::string defName   = item->data(0, Qt::UserRole).toString().toStdString();
    const std::string actorName = item->data(0, Qt::UserRole + 1).toString().toStdString();
    const bool        isSubActor = (item->parent() != nullptr);

    // Determine current "directed to" value so the dialog can pre-fill it
    std::string currentTarget;
    for (const auto& def : m_definitions)
    {
        if (def.name != defName) continue;
        if (isSubActor)
        {
            const auto it = def.subActorDirectedTo.find(actorName);
            currentTarget = (it != def.subActorDirectedTo.end())
                                ? it->second : def.directedTo;
        }
        else
        {
            currentTarget = def.directedTo;
        }
        break;
    }

    QMenu menu(this);
    const QString label = isSubActor
        ? tr("Set Directed To for '%1'…").arg(QString::fromStdString(actorName))
        : tr("Set Directed To…");
    menu.addAction(label);
    QAction* const clearAction = menu.addAction(tr("Clear Directed To"));
    clearAction->setEnabled(!currentTarget.empty());

    QAction* chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (!chosen) return;

    if (chosen == clearAction)
    {
        emit ActorDirectionChanged(
            QString::fromStdString(defName),
            QString::fromStdString(actorName),
            isSubActor,
            QString());
        return;
    }

    // ── "Set" dialog ──────────────────────────────────────────────────────
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Set Directed To"));
    dlg.setMinimumWidth(320);
    auto* layout = new QVBoxLayout(&dlg);

    auto* infoLabel = new QLabel(
        tr("Actor that <b>%1</b> sends events to:")
            .arg(QString::fromStdString(actorName)), &dlg);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    // Collect all known actor names for the combo (deduplicated)
    auto* combo = new QComboBox(&dlg);
    combo->setEditable(true);
    combo->addItem(tr("(none)"), QString());
    {
        std::set<std::string> seen;
        for (const auto& [dName, group] : m_groupedCache)
            for (const auto& [aName, data] : group.actors)
                if (aName != actorName && seen.insert(aName).second)
                    combo->addItem(QString::fromStdString(aName),
                                   QString::fromStdString(aName));
    }

    // Pre-fill with the current value
    if (currentTarget.empty())
    {
        combo->setCurrentIndex(0);
    }
    else
    {
        const int idx = combo->findData(QString::fromStdString(currentTarget));
        if (idx >= 0)
            combo->setCurrentIndex(idx);
        else
            combo->setEditText(QString::fromStdString(currentTarget));
    }
    layout->addWidget(combo);

    auto* btns = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(btns, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(btns, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(btns);

    if (dlg.exec() != QDialog::Accepted) return;

    const QString target = combo->currentText().trimmed();
    emit ActorDirectionChanged(
        QString::fromStdString(defName),
        QString::fromStdString(actorName),
        isSubActor,
        target == tr("(none)") ? QString() : target);
}

// ---------------------------------------------------------------------------
// Sequence diagram
// ---------------------------------------------------------------------------

void ActorsPanel::ShowSequenceDiagram()
{
    // ── 1. Determine which actor names to include ─────────────────────────
    // Use tree selection; fall back to all actors when nothing is selected.
    std::set<std::string> selectedActors;
    const QList<QTreeWidgetItem*> sel = m_tree->selectedItems();
    if (!sel.isEmpty())
    {
        for (QTreeWidgetItem* item : sel)
        {
            if (item->childCount() == 0)
            {
                // Leaf item (fixed-name actor or capture-group child)
                selectedActors.insert(
                    item->data(0, Qt::UserRole + 1).toString().toStdString());
            }
            else
            {
                // Group header: include all children
                for (int c = 0; c < item->childCount(); ++c)
                    selectedActors.insert(
                        item->child(c)->data(0, Qt::UserRole + 1)
                            .toString().toStdString());
            }
        }
    }
    else
    {
        for (const auto& [defName, group] : m_groupedCache)
            for (const auto& [actorName, data] : group.actors)
                selectedActors.insert(actorName);
    }

    if (selectedActors.empty())
    {
        QMessageBox::information(this, tr("Sequence Diagram"),
            tr("No actors available. Load a log file first."));
        return;
    }

    // ── 2. Build dialog ───────────────────────────────────────────────────
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(tr("Sequence Diagram — PlantUML"));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(740, 640);

    auto* mainLayout = new QVBoxLayout(dlg);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    // ── Options form ──────────────────────────────────────────────────────
    auto* form         = new QFormLayout();
    auto* senderEdit   = new QLineEdit("actor",  dlg);
    auto* receiverEdit = new QLineEdit("target", dlg);
    auto* labelEdit    = new QLineEdit("action", dlg);
    auto* maxSpin      = new QSpinBox(dlg);
    maxSpin->setRange(1, 5000);
    maxSpin->setValue(300);
    maxSpin->setSuffix(tr(" events"));
    senderEdit->setToolTip(tr("Log field used as the message sender"));
    receiverEdit->setToolTip(tr("Log field used as the message receiver"));
    labelEdit->setToolTip(tr(
        "Log field used as the arrow label (comma-separated list tried in order)"));
    form->addRow(tr("Sender field:"),   senderEdit);
    form->addRow(tr("Receiver field:"), receiverEdit);
    form->addRow(tr("Label field:"),    labelEdit);
    form->addRow(tr("Max events:"),     maxSpin);
    mainLayout->addLayout(form);

    // ── Output text ───────────────────────────────────────────────────────
    auto* output = new QPlainTextEdit(dlg);
    output->setReadOnly(true);
    output->setLineWrapMode(QPlainTextEdit::NoWrap);
    {
        QFont f = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        f.setPointSize(11);
        output->setFont(f);
    }
    mainLayout->addWidget(output, 1);

    // ── Buttons ───────────────────────────────────────────────────────────
    auto* btnLayout  = new QHBoxLayout();
    auto* genBtn     = new QPushButton(tr("Generate"), dlg);
    auto* copyBtn    = new QPushButton(tr("Copy"), dlg);
    auto* closeBtn   = new QPushButton(tr("Close"), dlg);
    genBtn->setDefault(true);
    btnLayout->addWidget(genBtn);
    btnLayout->addWidget(copyBtn);
    btnLayout->addStretch();
    btnLayout->addWidget(closeBtn);
    mainLayout->addLayout(btnLayout);

    // ── Generation lambda ─────────────────────────────────────────────────
    // IMPORTANT: capture widget pointers by VALUE. The dialog is non-modal, so
    // ShowSequenceDiagram() returns while dlg is still alive. Any [&] capture
    // of local pointer variables would dangle after this function returns.
    const std::map<std::string, GroupData> cachedGroups = m_groupedCache;
    const std::set<std::string>            actors       = selectedActors;

    auto generate = [senderEdit, receiverEdit, labelEdit, maxSpin, output,
                     cachedGroups, actors, this]()
    {
        const std::string senderField   = senderEdit->text().trimmed().toStdString();
        const std::string receiverField = receiverEdit->text().trimmed().toStdString();
        const std::string labelField    = labelEdit->text().trimmed().toStdString();
        const int         maxEvents     = maxSpin->value();

        if (senderField.empty() || receiverField.empty())
        {
            output->setPlainText(tr("// Sender and Receiver field names must not be empty."));
            return;
        }

        // Collect sorted event indices for selected actors
        std::set<unsigned long> indexSet;
        for (const auto& [defName, group] : cachedGroups)
            for (const auto& [actorName, data] : group.actors)
                if (actors.count(actorName))
                    for (unsigned long idx : data.indices)
                        indexSet.insert(idx);

        // Parse comma-separated label fields
        std::vector<std::string> labelFields;
        {
            std::istringstream ss(labelField);
            std::string tok;
            while (std::getline(ss, tok, ','))
            {
                while (!tok.empty() && tok.front() == ' ') tok.erase(tok.begin());
                while (!tok.empty() && tok.back()  == ' ') tok.pop_back();
                if (!tok.empty()) labelFields.push_back(tok);
            }
        }

        // Build PlantUML text
        QString puml = "@startuml\n";

        // Declare participants in a deterministic order
        for (const auto& a : actors)
            puml += QString("participant \"%1\"\n")
                        .arg(QString::fromStdString(a));
        puml += "\n";

        int written = 0;
        for (unsigned long idx : indexSet)
        {
            if (written >= maxEvents) break;

            const db::LogEvent& ev =
                m_events.GetEvent(static_cast<int>(idx));

            const std::string from = ev.findByKey(senderField);
            const std::string to   = ev.findByKey(receiverField);

            if (from.empty() || to.empty()) continue;
            // Only draw arrows where at least one side is a selected actor
            if (!actors.count(from) && !actors.count(to)) continue;

            // Pick first non-empty label field
            std::string label;
            for (const auto& lf : labelFields)
            {
                label = ev.findByKey(lf);
                if (!label.empty()) break;
            }
            if (label.empty()) label = "(event)";

            // Sanitise label: strip newlines
            for (char& c : label)
                if (c == '\n' || c == '\r') c = ' ';

            // Self-message or regular arrow
            const QString arrow = (from == to) ? "->" : "->>";
            puml += QString("\"%1\" %2 \"%3\" : %4\n")
                        .arg(QString::fromStdString(from),
                             arrow,
                             QString::fromStdString(to),
                             QString::fromStdString(label));
            ++written;
        }

        if (written == 0)
            puml += "' No matching events found with the given field names.\n";
        else if (written >= maxEvents && indexSet.size() > static_cast<size_t>(maxEvents))
            puml += QString("\n' Truncated — showing first %1 of %2 candidate events\n")
                        .arg(maxEvents)
                        .arg(indexSet.size());

        puml += "@enduml\n";
        output->setPlainText(puml);
    };

    connect(genBtn,  &QPushButton::clicked, dlg, generate);
    connect(copyBtn, &QPushButton::clicked, dlg, [output]() {
        QGuiApplication::clipboard()->setText(output->toPlainText());
    });
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);

    // Auto-generate on open
    generate();

    dlg->show();
}

} // namespace ui::qt
