#include "ActorDefinitionsPanel.hpp"

#include "analyzers/ActorDiscoverer.hpp"
#include "Config.hpp"
#include "EventsContainer.hpp"
#include "Logger.hpp"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <map>
#include <set>
#include <stdexcept>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRadioButton>
#include <QRegularExpression>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <fstream>
#include <filesystem>

namespace ui::qt {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ActorDefinitionsPanel::ActorDefinitionsPanel(QWidget* parent)
    : QWidget(parent)
{
    BuildLayout();
    m_currentFilePath = DefaultFilePath();

    // Auto-load definitions from the default path on startup
    util::Logger::Debug("[ActorDefinitionsPanel] Auto-loading from '{}'", m_currentFilePath);
    std::ifstream ifs(m_currentFilePath);
    if (ifs.is_open())
    {
        try
        {
            nlohmann::json j;
            ifs >> j;
            m_definitions = ActorDefinition::ListFromJson(j);
            RebuildTable();
            if (!m_definitions.empty())
            {
                util::Logger::Info("[ActorDefinitionsPanel] Auto-loaded {} definition(s) from '{}'",
                                   m_definitions.size(), m_currentFilePath);
                SetStatus(tr("Auto-loaded %1 definition(s)").arg(m_definitions.size()), false);
            }
            else
            {
                util::Logger::Warn("[ActorDefinitionsPanel] Auto-load: file '{}' contains 0 definitions",
                                   m_currentFilePath);
            }
        }
        catch (const std::exception& e)
        {
            util::Logger::Error("[ActorDefinitionsPanel] Auto-load failed for '{}': {}",
                                m_currentFilePath, e.what());
            SetStatus(tr("Auto-load failed: %1").arg(QString::fromStdString(e.what())), true);
        }
    }
}

void ActorDefinitionsPanel::BuildLayout()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto* infoLabel = new QLabel(
        tr("Define actors by name and a regular expression.\n"
           "Each matching event will be attributed to that actor.\n"
           "Enable \"Use captures\" to derive actor names from regexp capture groups."),
        this);
    infoLabel->setWordWrap(true);
    static const QString kTooltip = tr(
        "<b>How to define actors</b><br>"
        "<b>Name</b> — display label used in the Actors table.<br>"
        "<b>Field</b> — log field to match against (leave empty to search all fields).<br>"
        "<b>Pattern</b> — ECMAScript regular expression tested against the field value.<br>"
        "<b>Use captures</b> — when checked, each capture group <code>(…)</code> in the "
        "pattern produces a <em>separate actor</em> named after the captured text.<br>"
        "<br>"
        "<b>Examples</b><br>"
        "Simple match (no captures):<br>"
        "&nbsp;&nbsp;Name: <i>Auth service</i>, Field: <i>service</i>, Pattern: <code>auth.*</code><br>"
        "Capture groups (dynamic actors):<br>"
        "&nbsp;&nbsp;Name: <i>User</i>, Field: <i>message</i>, "
        "Pattern: <code>user=(\\w+)</code> — actor name = captured word<br>"
        "Multiple captures:<br>"
        "&nbsp;&nbsp;Pattern: <code>(alice|bob|carol)</code> — one actor per alternative match"
    );
    infoLabel->setToolTip(kTooltip);
    layout->addWidget(infoLabel);

    // ── Table ─────────────────────────────────────────────────────────────
    m_table = new QTableWidget(0, 8, this);
    m_table->setHorizontalHeaderLabels(
        {tr("On"), tr("Self"), tr("Name"), tr("Alias"),
         tr("Field"), tr("Pattern (regexp)"), tr("Captures"), tr("Directed To")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(6, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(7, QHeaderView::Interactive);
    m_table->horizontalHeader()->setToolTip(tr(
        "Self: marks this as the main actor (log generator).\n"
        "Captures: when ✓, each () group in the pattern produces a separate actor"));
    m_table->setColumnWidth(2, 100);
    m_table->setColumnWidth(3, 100);
    m_table->setColumnWidth(4, 80);
    m_table->setColumnWidth(7, 100);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->hide();
    m_table->setAlternatingRowColors(true);
    layout->addWidget(m_table, 1);

    // ── Status label ─────────────────────────────────────────────────────
    m_statusLabel = new QLabel(this);
    m_statusLabel->setWordWrap(true);
    m_statusLabel->setStyleSheet("font-style: italic;");
    layout->addWidget(m_statusLabel);

    // ── Buttons: Add / Edit / Remove ─────────────────────────────────────
    auto* btnRow1 = new QHBoxLayout();
    auto* addBtn  = new QPushButton(tr("Add"),    this);
    m_editBtn     = new QPushButton(tr("Edit"),   this);
    m_removeBtn   = new QPushButton(tr("Remove"), this);
    m_editBtn->setEnabled(false);
    m_removeBtn->setEnabled(false);
    btnRow1->addWidget(addBtn);
    btnRow1->addWidget(m_editBtn);
    btnRow1->addWidget(m_removeBtn);
    layout->addLayout(btnRow1);

    // ── Buttons: Save / Save As / Load ───────────────────────────────────
    auto* btnRow2     = new QHBoxLayout();
    auto* saveBtn     = new QPushButton(tr("Save"),        this);
    auto* saveAsBtn   = new QPushButton(tr("Save As…"),    this);
    auto* loadBtn     = new QPushButton(tr("Load…"),       this);
    auto* discoverBtn = new QPushButton(tr("Discover…"),   this);
    discoverBtn->setToolTip(tr(
        "Scan the loaded log data and suggest actor fields automatically.\n"
        "Fields with a moderate number of distinct values (2–200) are candidates.\n"
        "You can review, select, and import the suggestions."));
    btnRow2->addWidget(saveBtn);
    btnRow2->addWidget(saveAsBtn);
    btnRow2->addWidget(loadBtn);
    btnRow2->addWidget(discoverBtn);
    layout->addLayout(btnRow2);

    // ── Buttons: Apply Filter / Clear Filter ─────────────────────────────
    auto* btnRow3      = new QHBoxLayout();
    m_applyFilterBtn   = new QPushButton(tr("Apply Filter"), this);
    m_clearFilterBtn   = new QPushButton(tr("Clear Filter"), this);
    m_applyFilterBtn->setToolTip(tr("Filter the events view to only show events "
                                    "matching the enabled actor definitions"));
    m_clearFilterBtn->setToolTip(tr("Remove the actor filter and show all events"));
    m_applyFilterBtn->setDefault(false);
    btnRow3->addWidget(m_applyFilterBtn);
    btnRow3->addWidget(m_clearFilterBtn);
    layout->addLayout(btnRow3);

    // ── Connections ───────────────────────────────────────────────────────
    connect(addBtn,     &QPushButton::clicked, this, &ActorDefinitionsPanel::HandleAdd);
    connect(m_editBtn,  &QPushButton::clicked, this, &ActorDefinitionsPanel::HandleEdit);
    connect(m_removeBtn,&QPushButton::clicked, this, &ActorDefinitionsPanel::HandleRemove);
    connect(saveBtn,    &QPushButton::clicked, this, &ActorDefinitionsPanel::HandleSave);
    connect(saveAsBtn,  &QPushButton::clicked, this, &ActorDefinitionsPanel::HandleSaveAs);
    connect(loadBtn,    &QPushButton::clicked, this, &ActorDefinitionsPanel::HandleLoad);
    connect(discoverBtn,&QPushButton::clicked, this, &ActorDefinitionsPanel::HandleDiscover);
    connect(m_table,    &QTableWidget::itemSelectionChanged,
            this, &ActorDefinitionsPanel::HandleSelectionChanged);
    connect(m_table,    &QTableWidget::cellDoubleClicked,
            this, [this](int, int) { HandleEdit(); });
    connect(m_table,    &QTableWidget::itemChanged,
            this, &ActorDefinitionsPanel::HandleItemChanged);
    connect(m_applyFilterBtn, &QPushButton::clicked,
            this, &ActorDefinitionsPanel::RequestApplyFilter);
    connect(m_clearFilterBtn, &QPushButton::clicked,
            this, &ActorDefinitionsPanel::RequestClearFilter);
}

// ---------------------------------------------------------------------------
// Table population
// ---------------------------------------------------------------------------

void ActorDefinitionsPanel::RebuildTable()
{
    m_rebuilding = true;
    m_table->setRowCount(0);
    for (const auto& def : m_definitions)
    {
        const int row = m_table->rowCount();
        m_table->insertRow(row);

        // Col 0 \u2014 Enabled checkbox
        auto* onItem = new QTableWidgetItem();
        onItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        onItem->setCheckState(def.enabled ? Qt::Checked : Qt::Unchecked);
        m_table->setItem(row, 0, onItem);

        // Col 1 \u2014 Self (main actor) indicator, read-only (edit via Edit dialog)
        auto* selfItem = new QTableWidgetItem(def.isSelf ? tr("\u2605") : QString());
        selfItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        selfItem->setTextAlignment(Qt::AlignCenter);
        selfItem->setToolTip(def.isSelf
            ? tr("This is the main/self actor (log generator).\n"
                 "Sequence diagram: outgoing arrows are green, incoming are blue.")
            : tr("Not the main actor. Use Edit to mark as self."));
        if (def.isSelf)
            selfItem->setForeground(QColor(0, 120, 0));
        m_table->setItem(row, 1, selfItem);

        // Col 2 \u2014 Name
        m_table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(def.name)));
        // Col 3 \u2014 Alias
        auto* aliasItem = new QTableWidgetItem(QString::fromStdString(def.alias));
        aliasItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        aliasItem->setToolTip(def.alias.empty()
            ? tr("No alias \u2014 actor name used as-is in diagram")
            : tr("Sequence diagram shows this alias instead of the raw actor name"));
        m_table->setItem(row, 3, aliasItem);
        // Col 4 \u2014 Field
        m_table->setItem(row, 4, new QTableWidgetItem(QString::fromStdString(def.field)));
        // Col 5 \u2014 Pattern
        m_table->setItem(row, 5, new QTableWidgetItem(QString::fromStdString(def.pattern)));

        // Col 6 \u2014 Captures
        auto* capItem = new QTableWidgetItem(def.useCaptures ? tr("\u2713") : QString());
        capItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        capItem->setTextAlignment(Qt::AlignCenter);
        capItem->setToolTip(def.useCaptures
            ? tr("Each capture group () produces a separate actor")
            : tr("Actor name is fixed (no capture groups used)"));
        m_table->setItem(row, 6, capItem);

        // Col 7 \u2014 Directed To
        auto* directedToItem = new QTableWidgetItem(QString::fromStdString(def.directedTo));
        directedToItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        directedToItem->setToolTip(def.directedTo.empty()
            ? tr("No target actor specified")
            : tr("Events from this actor are directed to: %1")
                  .arg(QString::fromStdString(def.directedTo)));
        m_table->setItem(row, 7, directedToItem);

        if (!def.enabled)
        {
            for (int c = 2; c < 8; ++c)
                if (auto* it = m_table->item(row, c))
                    it->setForeground(QColor(150, 150, 150));
        }
    }
    m_rebuilding = false;
    HandleSelectionChanged();
}

// ---------------------------------------------------------------------------
// Slots
// ---------------------------------------------------------------------------

void ActorDefinitionsPanel::HandleAdd()
{
    ActorDefinition def;
    if (!EditDefinition(def, /*isNew=*/true)) return;
    util::Logger::Info("[ActorDefinitionsPanel] Added definition '{}'", def.name);
    m_definitions.push_back(def);
    RebuildTable();
    EmitAndSave();
}

void ActorDefinitionsPanel::HandleEdit()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= static_cast<int>(m_definitions.size())) return;

    // Sync enabled state from table checkbox before editing
    if (auto* it = m_table->item(row, 0))
        m_definitions[static_cast<size_t>(row)].enabled =
            (it->checkState() == Qt::Checked);

    ActorDefinition def = m_definitions[static_cast<size_t>(row)];
    if (!EditDefinition(def, /*isNew=*/false)) return;
    util::Logger::Info("[ActorDefinitionsPanel] Edited definition '{}' (row {})", def.name, row);
    m_definitions[static_cast<size_t>(row)] = def;
    RebuildTable();
    EmitAndSave();
}

void ActorDefinitionsPanel::HandleRemove()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= static_cast<int>(m_definitions.size())) return;

    const QString name =
        QString::fromStdString(m_definitions[static_cast<size_t>(row)].name);
    if (QMessageBox::question(this, tr("Confirm"),
            tr("Remove actor '%1'?").arg(name)) != QMessageBox::Yes)
        return;

    util::Logger::Info("[ActorDefinitionsPanel] Removed definition '{}'", name.toStdString());
    m_definitions.erase(m_definitions.begin() + row);
    RebuildTable();
    EmitAndSave();
}

void ActorDefinitionsPanel::HandleSave()
{
    if (m_currentFilePath.empty())
    {
        HandleSaveAs();
        return;
    }
    util::Logger::Debug("[ActorDefinitionsPanel] HandleSave: {} definition(s) to '{}'",
                        m_definitions.size(), m_currentFilePath);

    // Sync enabled checkboxes before saving
    for (int r = 0; r < m_table->rowCount(); ++r)
    {
        if (auto* it = m_table->item(r, 0))
            m_definitions[static_cast<size_t>(r)].enabled =
                (it->checkState() == Qt::Checked);
    }

    try
    {
        std::filesystem::create_directories(
            std::filesystem::path(m_currentFilePath).parent_path());

        std::ofstream ofs(m_currentFilePath);
        if (!ofs) throw std::runtime_error("Cannot open file for writing");
        const std::string json = ActorDefinition::ListToJson(m_definitions).dump(2);
        ofs << json;
        ofs.flush();
        if (!ofs) throw std::runtime_error("Write error");
        util::Logger::Info("[ActorDefinitionsPanel] Saved {} definition(s) to '{}'",
                           m_definitions.size(), m_currentFilePath);
        emit DefinitionsChanged(m_definitions);
        SetStatus(tr("Saved %1 definition(s) to %2.")
            .arg(m_definitions.size())
            .arg(QString::fromStdString(m_currentFilePath)), false);
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[ActorDefinitionsPanel] Save failed for '{}': {}",
                            m_currentFilePath, e.what());
        QMessageBox::warning(this, tr("Save Error"),
            tr("Could not save actors: %1").arg(QString::fromStdString(e.what())));
        return;
    }

    // Keep actors.json in sync regardless of where we saved
    const std::string defaultPath = DefaultFilePath();
    if (m_currentFilePath != defaultPath)
    {
        try
        {
            std::filesystem::create_directories(
                std::filesystem::path(defaultPath).parent_path());
            std::ofstream def(defaultPath);
            if (def)
            {
                def << ActorDefinition::ListToJson(m_definitions).dump(2);
                def.flush();
            }
        }
        catch (...) {} // best-effort
    }
}

void ActorDefinitionsPanel::HandleSaveAs()
{
    QFileDialog dlg(this, tr("Save Actor Definitions"));
#ifdef __APPLE__
    dlg.setOption(QFileDialog::DontUseNativeDialog, true);
#endif
    dlg.setNameFilter(tr("Actor Definitions (*.json)"));
    dlg.setAcceptMode(QFileDialog::AcceptSave);
    dlg.setDefaultSuffix("json");

    // Start in the same directory as the current file
    const std::string startFile = m_currentFilePath.empty()
        ? DefaultFilePath()
        : m_currentFilePath;
    dlg.setDirectory(QString::fromStdString(
        std::filesystem::path(startFile).parent_path().string()));
    dlg.selectFile(QString::fromStdString(
        std::filesystem::path(startFile).filename().string()));

    if (dlg.exec() != QDialog::Accepted) return;
    const QString path = dlg.selectedFiles().value(0);
    if (path.isEmpty()) return;

    util::Logger::Info("[ActorDefinitionsPanel] Save As: '{}'", path.toStdString());
    m_currentFilePath = path.toStdString();
    HandleSave();
}

void ActorDefinitionsPanel::HandleLoad()
{
    QFileDialog dlg(this, tr("Load Actor Definitions"));
#ifdef __APPLE__
    dlg.setOption(QFileDialog::DontUseNativeDialog, true);
#endif
    dlg.setNameFilter(tr("Actor Definitions (*.json)"));
    dlg.setAcceptMode(QFileDialog::AcceptOpen);
    dlg.setFileMode(QFileDialog::ExistingFile);

    // Start in the directory of the currently loaded file (or default app path)
    const std::string startDir = m_currentFilePath.empty()
        ? DefaultFilePath()
        : m_currentFilePath;
    dlg.setDirectory(QString::fromStdString(
        std::filesystem::path(startDir).parent_path().string()));

    if (dlg.exec() != QDialog::Accepted) return;
    const QString path = dlg.selectedFiles().value(0);
    if (path.isEmpty()) return;

    util::Logger::Debug("[ActorDefinitionsPanel] HandleLoad: '{}'", path.toStdString());
    try
    {
        std::ifstream ifs(path.toStdString());
        if (!ifs) throw std::runtime_error("Cannot open file");
        nlohmann::json j;
        ifs >> j;
        m_definitions = ActorDefinition::ListFromJson(j);
        m_currentFilePath = path.toStdString();
        RebuildTable();
        emit DefinitionsChanged(m_definitions);
        if (m_definitions.empty())
        {
            util::Logger::Warn("[ActorDefinitionsPanel] Loaded file '{}' contains 0 definitions",
                               path.toStdString());
            SetStatus(tr("File loaded but contains 0 definitions."), true);
        }
        else
        {
            util::Logger::Info("[ActorDefinitionsPanel] Loaded {} definition(s) from '{}'",
                               m_definitions.size(), path.toStdString());
            SetStatus(tr("Loaded %1 definition(s) from file.").arg(m_definitions.size()), false);
        }

        // Keep actors.json in sync so the loaded definitions survive restarts
        const std::string defaultPath = DefaultFilePath();
        if (m_currentFilePath != defaultPath)
        {
            try
            {
                std::filesystem::create_directories(
                    std::filesystem::path(defaultPath).parent_path());
                std::ofstream def(defaultPath);
                if (def)
                {
                    def << ActorDefinition::ListToJson(m_definitions).dump(2);
                    def.flush();
                }
            }
            catch (...) {} // best-effort
        }
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[ActorDefinitionsPanel] Load failed for '{}': {}",
                            path.toStdString(), e.what());
        QMessageBox::warning(this, tr("Load Error"),
            tr("Could not load actors: %1").arg(QString::fromStdString(e.what())));
    }
}

void ActorDefinitionsPanel::HandleDiscover()
{
    if (!m_events || m_events->Size() == 0)
    {
        util::Logger::Warn("[ActorDefinitionsPanel] Discover requested but no log data loaded");
        QMessageBox::information(this, tr("Discover Actors"),
            tr("No log data is loaded. Open a log file first."));
        return;
    }
    util::Logger::Debug("[ActorDefinitionsPanel] HandleDiscover: scanning {} event(s)",
                        m_events->Size());

    // ── Ask user which discovery method to use ─────────────────────────────
    QDialog methodDlg(this);
    methodDlg.setWindowTitle(tr("Discovery Method"));
    methodDlg.setMinimumWidth(400);
    auto* methodLayout = new QVBoxLayout(&methodDlg);

    auto* label = new QLabel(
        tr("Choose how to discover actor fields:"), &methodDlg);
    methodLayout->addWidget(label);

    auto* heuristicRadio = new QRadioButton(
        tr("Heuristic: Fast, keyword-based analysis"), &methodDlg);
    auto* aiRadio = new QRadioButton(
        tr("AI: Uses LLM for pattern analysis (if model available)"), &methodDlg);
    heuristicRadio->setChecked(true);

    methodLayout->addWidget(heuristicRadio);
    methodLayout->addWidget(aiRadio);
    methodLayout->addStretch();

    auto* methodButtons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &methodDlg);
    connect(methodButtons, &QDialogButtonBox::accepted, &methodDlg, &QDialog::accept);
    connect(methodButtons, &QDialogButtonBox::rejected, &methodDlg, &QDialog::reject);
    methodLayout->addWidget(methodButtons);

    if (methodDlg.exec() != QDialog::Accepted) return;

    bool useAIDiscovery = aiRadio->isChecked();

    // ── Run appropriate discovery method ────────────────────────────────────
    analyzer::ActorDiscoveryResult discovered;
    if (useAIDiscovery)
    {
        util::Logger::Debug("[ActorDefinitionsPanel] Using AI discovery method");
        discovered = analyzer::ActorDiscoverer::DiscoverWithAI(*m_events);
    }
    else
    {
        util::Logger::Debug("[ActorDefinitionsPanel] Using heuristic discovery method");
        discovered = analyzer::ActorDiscoverer::Discover(*m_events);
    }

    // ── Collect all candidate field names: actor fields + exchange pattern fields
    std::vector<std::string> candidateFields = discovered.actorFields;
    for (const auto& pat : discovered.patterns)
    {
        for (const auto& f : {pat.senderField,
                               pat.receiverField,
                               pat.actorField,
                               pat.labelField})
        {
            if (!f.empty() &&
                std::find(candidateFields.begin(), candidateFields.end(), f) == candidateFields.end())
                candidateFields.push_back(f);
        }
    }

    // Skip fields already covered by existing definitions
    candidateFields.erase(
        std::remove_if(candidateFields.begin(), candidateFields.end(),
            [this](const std::string& f) {
                for (const auto& def : m_definitions)
                    if (def.field == f) return true;
                return false;
            }),
        candidateFields.end());

    // Build scored candidates with sample values (small probe for display)
    struct Candidate {
        std::string field;
        int         uniqueCount {0};
        int         score       {0};
        std::vector<std::string> samples;
    };
    std::vector<Candidate> candidates;
    candidates.reserve(candidateFields.size());

    for (const auto& field : candidateFields)
    {
        Candidate c;
        c.field = field;
        c.score = analyzer::ActorDiscoverer::ScoreActor(field) +
                  analyzer::ActorDiscoverer::ScoreSender(field) +
                  analyzer::ActorDiscoverer::ScoreReceiver(field);

        // Collect up to 5 sample values from the first 500 events
        std::set<std::string> seen;
        const size_t probe = (std::min)(m_events->Size(), size_t(500));
        for (size_t i = 0; i < probe && static_cast<int>(c.samples.size()) < 5; ++i)
        {
            try {
                const std::string v = m_events->GetEvent(i).findByKey(field);
                if (!v.empty() && seen.insert(v).second)
                    c.samples.push_back(v);
            } catch (const std::out_of_range&) { break; }
        }
        c.uniqueCount = static_cast<int>(seen.size());
        candidates.push_back(std::move(c));
    }

    std::sort(candidates.begin(), candidates.end(),
        [](const Candidate& a, const Candidate& b) {
            if (a.score != b.score) return a.score > b.score;
            return a.uniqueCount < b.uniqueCount;
        });

    if (candidates.empty())
    {
        util::Logger::Warn("[ActorDefinitionsPanel] Discover: no suitable actor fields found");
        QMessageBox::information(this, tr("Discover Actors"),
            tr("No suitable actor fields found in the loaded log data.\n\n"
               "Suitable fields have between 2 and 200 distinct values.\n"
               "Fields already covered by existing definitions are skipped."));
        return;
    }
    util::Logger::Debug("[ActorDefinitionsPanel] Discover: {} candidate field(s) found",
                        candidates.size());

    // ── Field selection dialog ────────────────────────────────────────────
    QDialog dlg(this);
    dlg.setWindowTitle(tr("Discovered Actor Fields"));
    dlg.setMinimumWidth(560);
    dlg.setMinimumHeight(400);

    auto* mainLayout = new QVBoxLayout(&dlg);

    auto* methodInfo = new QLabel(
        QString(tr("Using %1 discovery method"))
            .arg(useAIDiscovery ? tr("AI") : tr("heuristic")), &dlg);
    methodInfo->setStyleSheet("font-weight: bold; color: #0066cc;");
    mainLayout->addWidget(methodInfo);

    auto* info = new QLabel(
        tr("The following fields were found in the log data with a moderate number of "
           "distinct values. Select the fields you want to import as actor definitions.\n"
           "Each selected field will be imported with pattern <tt>(.+)</tt> and "
           "\"Use captures\" enabled, so every distinct value becomes its own actor."), &dlg);
    info->setWordWrap(true);
    info->setTextFormat(Qt::RichText);
    mainLayout->addWidget(info);

    auto* candTable = new QTableWidget(static_cast<int>(candidates.size()), 4, &dlg);
    candTable->setHorizontalHeaderLabels(
        {tr("Import"), tr("Field"), tr("Unique values"), tr("Sample values")});
    candTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    candTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    candTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    candTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    candTable->setColumnWidth(1, 130);
    candTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    candTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    candTable->verticalHeader()->hide();
    candTable->setAlternatingRowColors(true);

    for (int r = 0; r < static_cast<int>(candidates.size()); ++r)
    {
        const auto& c = candidates[static_cast<size_t>(r)];

        auto* checkItem = new QTableWidgetItem();
        checkItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        checkItem->setCheckState(c.score >= 10 ? Qt::Checked : Qt::Unchecked);
        candTable->setItem(r, 0, checkItem);

        candTable->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(c.field)));
        candTable->setItem(r, 2, new QTableWidgetItem(QString::number(c.uniqueCount)));

        QString sampleText;
        for (const auto& s : c.samples)
        {
            if (!sampleText.isEmpty()) sampleText += ", ";
            sampleText += QString::fromStdString(s);
        }
        candTable->setItem(r, 3, new QTableWidgetItem(sampleText));
    }
    mainLayout->addWidget(candTable, 1);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    buttons->button(QDialogButtonBox::Ok)->setText(tr("Import Selected"));
    connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    mainLayout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) return;

    // ── Import selected fields ────────────────────────────────────────────
    int imported = 0;
    for (int r = 0; r < candTable->rowCount(); ++r)
    {
        const auto* checkItem = candTable->item(r, 0);
        if (!checkItem || checkItem->checkState() != Qt::Checked) continue;

        const std::string field = candidates[static_cast<size_t>(r)].field;

        ActorDefinition def;
        def.name        = field;
        def.field       = field;
        def.pattern     = "(.+)";
        def.useCaptures = true;
        def.enabled     = true;
        m_definitions.push_back(def);
        ++imported;
    }

    if (imported > 0)
    {
        util::Logger::Info("[ActorDefinitionsPanel] Discover: imported {} definition(s)", imported);
        RebuildTable();
        EmitAndSave();
        SetStatus(tr("Imported %1 actor definition(s) from discovery.").arg(imported), false);
    }
    else
    {
        util::Logger::Debug("[ActorDefinitionsPanel] Discover: no fields selected for import");
        SetStatus(tr("No fields selected for import."), false);
    }
}

void ActorDefinitionsPanel::HandleSelectionChanged()
{
    const bool has = (m_table->currentRow() >= 0);
    m_editBtn->setEnabled(has);
    m_removeBtn->setEnabled(has);
}

void ActorDefinitionsPanel::HandleItemChanged(QTableWidgetItem* item)
{
    if (m_rebuilding) return; // ignore signals during table rebuild
    if (!item || item->column() != 0) return;
    const int row = item->row();
    if (row < 0 || row >= static_cast<int>(m_definitions.size())) return;

    const bool on = (item->checkState() == Qt::Checked);
    util::Logger::Debug("[ActorDefinitionsPanel] Definition '{}' {}",
                        m_definitions[static_cast<size_t>(row)].name,
                        on ? "enabled" : "disabled");
    m_definitions[static_cast<size_t>(row)].enabled = on;

    // Update row text colour to reflect enabled state (skip col 0 = checkbox, col 1 = self)
    {
        m_rebuilding = true;
        for (int c = 2; c < 8; ++c)
            if (auto* it = m_table->item(row, c))
                it->setForeground(on ? QColor() : QColor(150, 150, 150));
        m_rebuilding = false;
    }

    EmitAndSave();
}

// ---------------------------------------------------------------------------
// Edit dialog
// ---------------------------------------------------------------------------

bool ActorDefinitionsPanel::EditDefinition(ActorDefinition& def, bool isNew)
{
    QDialog dlg(this);
    dlg.setWindowTitle(isNew ? tr("Add Actor") : tr("Edit Actor"));
    dlg.setMinimumWidth(480);

    // ── Guideline box ─────────────────────────────────────────────────────
    auto* guideBox    = new QGroupBox(tr("How actor matching works"), &dlg);
    auto* guideLayout = new QVBoxLayout(guideBox);
    guideLayout->setSpacing(4);

    auto makeGuideLabel = [&](const QString& text) {
        auto* lbl = new QLabel(text, guideBox);
        lbl->setWordWrap(true);
        lbl->setTextFormat(Qt::RichText);
        return lbl;
    };

    guideLayout->addWidget(makeGuideLabel(
        tr("<b>Mode 1 – Fixed name</b> (default)<br>"
           "The pattern is matched against the chosen field (or all fields). "
           "Every matching event is attributed to the actor <i>Name</i> you enter above.<br>"
           "<i>Example:</i> Name&nbsp;=&nbsp;<tt>Auth</tt>, "
           "Pattern&nbsp;=&nbsp;<tt>auth.*service</tt>")));

    auto* sep = new QFrame(guideBox);
    sep->setFrameShape(QFrame::HLine);
    sep->setFrameShadow(QFrame::Sunken);
    guideLayout->addWidget(sep);

    guideLayout->addWidget(makeGuideLabel(
        tr("<b>Mode 2 – Capture groups</b> (check the box below)<br>"
           "Each <tt>(&hellip;)</tt> group in the pattern becomes a <i>separate actor</i> "
           "named after the text it captured. The Name field is used only as a label "
           "in the definitions list.<br>"
           "<i>Examples:</i><br>"
           "&nbsp;&bull;&nbsp;<tt>user=(\\w+)</tt> &rarr; actor = word after <tt>user=</tt><br>"
           "&nbsp;&bull;&nbsp;<tt>(alice|bob|carol)</tt> &rarr; one actor per alternative<br>"
           "&nbsp;&bull;&nbsp;<tt>host-(\\d+)</tt> &rarr; actor = the captured digits")));

    // ── Form fields ───────────────────────────────────────────────────────
    auto* form = new QFormLayout();

    auto* nameEdit    = new QLineEdit(QString::fromStdString(def.name),    &dlg);
    auto* aliasEdit   = new QLineEdit(QString::fromStdString(def.alias),   &dlg);
    aliasEdit->setPlaceholderText(tr("(optional — shown in sequence diagram instead of raw name)"));
    aliasEdit->setToolTip(tr(
        "Friendly display name used in the sequence diagram.\n"
        "Leave empty to use the raw actor name / field value.\n"
        "For capture-group definitions, set per-value aliases via the Capture Aliases button."));
    auto* fieldEdit   = new QLineEdit(QString::fromStdString(def.field),   &dlg);
    auto* patternEdit = new QLineEdit(QString::fromStdString(def.pattern), &dlg);
    auto* selfBox     = new QCheckBox(tr("Mark as self / main actor (log generator)"), &dlg);
    selfBox->setChecked(def.isSelf);
    selfBox->setToolTip(tr(
        "Marks this actor as the perspective from which the log was recorded.\n"
        "Sequence diagram: outgoing arrows (from self) are shown in green;\n"
        "incoming arrows (to self) are shown in blue."));
    auto* enabledBox  = new QCheckBox(tr("Enabled"), &dlg);
    enabledBox->setChecked(def.enabled);
    auto* captureBox  = new QCheckBox(tr("Use capture groups as actors"), &dlg);
    captureBox->setChecked(def.useCaptures);
    captureBox->setToolTip(tr(
        "When checked, each () group in the pattern produces a separate actor "
        "named after the captured text. Requires at least one capture group."));

    // ── Directed-to combo ─────────────────────────────────────────────────
    auto* directedToCombo = new QComboBox(&dlg);
    directedToCombo->setEditable(true);
    directedToCombo->setInsertPolicy(QComboBox::NoInsert);
    directedToCombo->addItem(tr("(none)"), QString());
    {
        std::set<std::string> seen;
        for (const auto& d : m_definitions)
        {
            if (d.name == def.name) continue; // exclude self
            if (!seen.insert(d.name).second) continue; // deduplicate same-name defs
            const QString dname = QString::fromStdString(d.name);
            directedToCombo->addItem(dname, dname);
        }
    }
    // Pre-select the current value
    if (def.directedTo.empty())
    {
        directedToCombo->setCurrentIndex(0);
    }
    else
    {
        const int idx = directedToCombo->findData(QString::fromStdString(def.directedTo));
        if (idx >= 0)
            directedToCombo->setCurrentIndex(idx);
        else
            directedToCombo->setEditText(QString::fromStdString(def.directedTo));
    }
    directedToCombo->setToolTip(tr(
        "Actor that receives events from this actor. "
        "Shown as '→ target' next to the actor name in the Actors panel."));

    fieldEdit->setPlaceholderText(tr("(empty = any field)"));
    patternEdit->setPlaceholderText(tr("Regular expression …"));

    auto* statusLabel = new QLabel(&dlg);
    statusLabel->setWordWrap(true);

    form->addRow(tr("Name:"),        nameEdit);
    form->addRow(tr("Alias:"),       aliasEdit);
    form->addRow(tr("Field:"),       fieldEdit);
    form->addRow(tr("Pattern:"),     patternEdit);
    form->addRow(QString(),          captureBox);
    form->addRow(tr("Directed to:"), directedToCombo);
    form->addRow(QString(),          selfBox);
    form->addRow(QString(),          enabledBox);
    form->addRow(QString(),          statusLabel);

    // Live regexp validation + capture hint
    auto validate = [&]() {
        const QString pat = patternEdit->text().trimmed();
        if (pat.isEmpty())
        {
            statusLabel->setText(QString());
            statusLabel->setStyleSheet(QString());
            return;
        }
        QRegularExpression re(pat);
        if (!re.isValid())
        {
            statusLabel->setText(tr("✗ ") + re.errorString());
            statusLabel->setStyleSheet("color: red;");
            return;
        }
        const int groups = re.captureCount();
        if (groups > 0 && !captureBox->isChecked())
        {
            statusLabel->setText(
                tr("✓ Valid — pattern has %1 capture group(s). "
                   "Check \"Use capture groups\" to use them as actor names.")
                .arg(groups));
            statusLabel->setStyleSheet("color: orange;");
        }
        else if (groups == 0 && captureBox->isChecked())
        {
            statusLabel->setText(
                tr("⚠ No capture groups found. "
                   "Add (...) groups or uncheck \"Use capture groups\"."));
            statusLabel->setStyleSheet("color: orange;");
        }
        else
        {
            statusLabel->setText(tr("✓ Valid regexp"));
            statusLabel->setStyleSheet("color: green;");
        }
    };
    connect(patternEdit, &QLineEdit::textChanged, &dlg, validate);
    connect(captureBox, &QCheckBox::toggled, &dlg, [&](bool) { validate(); });
    validate();

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    connect(buttons, &QDialogButtonBox::accepted, &dlg, [&]() {
        if (nameEdit->text().trimmed().isEmpty())
        {
            QMessageBox::warning(&dlg, tr("Validation"), tr("Name must not be empty."));
            return;
        }
        const QRegularExpression re(patternEdit->text().trimmed());
        if (!re.isValid())
        {
            QMessageBox::warning(&dlg, tr("Validation"),
                tr("Pattern is not a valid regular expression:\n%1").arg(re.errorString()));
            return;
        }
        if (captureBox->isChecked() && re.captureCount() == 0)
        {
            QMessageBox::warning(&dlg, tr("Validation"),
                tr("\"Use capture groups\" is enabled but the pattern contains no capture groups.\n"
                   "Add at least one group with parentheses, e.g. (\\w+), or disable this option."));
            return;
        }
        dlg.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto* mainLayout = new QVBoxLayout(&dlg);
    mainLayout->addWidget(guideBox);
    mainLayout->addLayout(form);

    // ── Subactor directions table (visible only in capture-group mode) ────
    auto* subActorGroup = new QGroupBox(
        tr("Subactor Directed-To Overrides"), &dlg);
    subActorGroup->setToolTip(tr(
        "For each captured subactor name, specify which actor it sends events to.\n"
        "Overrides the definition-level 'Directed to' for that specific subactor.\n"
        "Right-click actors in the Actors panel to set these interactively."));
    subActorGroup->setVisible(captureBox->isChecked());
    auto* subActorLayout = new QVBoxLayout(subActorGroup);

    auto* subActorTable = new QTableWidget(0, 2, subActorGroup);
    subActorTable->setHorizontalHeaderLabels(
        {tr("Subactor Name"), tr("Directed To")});
    subActorTable->horizontalHeader()->setSectionResizeMode(
        0, QHeaderView::Stretch);
    subActorTable->horizontalHeader()->setSectionResizeMode(
        1, QHeaderView::Stretch);
    subActorTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    subActorTable->setSelectionMode(QAbstractItemView::SingleSelection);
    subActorTable->verticalHeader()->hide();
    subActorTable->setAlternatingRowColors(true);

    // Pre-populate from existing subActorDirectedTo
    for (const auto& [k, v] : def.subActorDirectedTo)
    {
        const int r = subActorTable->rowCount();
        subActorTable->insertRow(r);
        subActorTable->setItem(r, 0,
            new QTableWidgetItem(QString::fromStdString(k)));
        subActorTable->setItem(r, 1,
            new QTableWidgetItem(QString::fromStdString(v)));
    }
    subActorLayout->addWidget(subActorTable);

    auto* subBtnRow    = new QHBoxLayout();
    auto* addSubBtn    = new QPushButton(tr("Add"),    subActorGroup);
    auto* removeSubBtn = new QPushButton(tr("Remove"), subActorGroup);
    subBtnRow->addWidget(addSubBtn);
    subBtnRow->addWidget(removeSubBtn);
    subBtnRow->addStretch();
    subActorLayout->addLayout(subBtnRow);

    connect(addSubBtn, &QPushButton::clicked, &dlg, [subActorTable]() {
        const int r = subActorTable->rowCount();
        subActorTable->insertRow(r);
        subActorTable->setItem(r, 0, new QTableWidgetItem(QString()));
        subActorTable->setItem(r, 1, new QTableWidgetItem(QString()));
        subActorTable->editItem(subActorTable->item(r, 0));
    });
    connect(removeSubBtn, &QPushButton::clicked, &dlg, [subActorTable]() {
        const int r = subActorTable->currentRow();
        if (r >= 0) subActorTable->removeRow(r);
    });

    // ── Capture aliases table (visible only in capture-group mode) ───────
    auto* aliasGroup = new QGroupBox(tr("Capture Value Aliases"), &dlg);
    aliasGroup->setToolTip(tr(
        "Map raw captured log values to friendly display names.\n"
        "Used in the sequence diagram instead of the raw field value.\n"
        "Example: svc_auth_v2_prod → Auth Service"));
    aliasGroup->setVisible(captureBox->isChecked());
    auto* aliasLayout = new QVBoxLayout(aliasGroup);

    auto* aliasTable = new QTableWidget(0, 2, aliasGroup);
    aliasTable->setHorizontalHeaderLabels({tr("Raw Value (from log)"), tr("Display Alias")});
    aliasTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    aliasTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    aliasTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    aliasTable->setSelectionMode(QAbstractItemView::SingleSelection);
    aliasTable->verticalHeader()->hide();
    aliasTable->setAlternatingRowColors(true);

    for (const auto& [k, v] : def.captureAliases)
    {
        const int r = aliasTable->rowCount();
        aliasTable->insertRow(r);
        aliasTable->setItem(r, 0, new QTableWidgetItem(QString::fromStdString(k)));
        aliasTable->setItem(r, 1, new QTableWidgetItem(QString::fromStdString(v)));
    }
    aliasLayout->addWidget(aliasTable);

    auto* aliasBtnRow    = new QHBoxLayout();
    auto* addAliasBtn    = new QPushButton(tr("Add"),    aliasGroup);
    auto* removeAliasBtn = new QPushButton(tr("Remove"), aliasGroup);
    aliasBtnRow->addWidget(addAliasBtn);
    aliasBtnRow->addWidget(removeAliasBtn);
    aliasBtnRow->addStretch();
    aliasLayout->addLayout(aliasBtnRow);

    connect(addAliasBtn, &QPushButton::clicked, &dlg, [aliasTable]() {
        const int r = aliasTable->rowCount();
        aliasTable->insertRow(r);
        aliasTable->setItem(r, 0, new QTableWidgetItem(QString()));
        aliasTable->setItem(r, 1, new QTableWidgetItem(QString()));
        aliasTable->editItem(aliasTable->item(r, 0));
    });
    connect(removeAliasBtn, &QPushButton::clicked, &dlg, [aliasTable]() {
        const int r = aliasTable->currentRow();
        if (r >= 0) aliasTable->removeRow(r);
    });

    // Show / hide groups when capture-group mode is toggled
    connect(captureBox, &QCheckBox::toggled, &dlg,
            [subActorGroup, aliasGroup](bool checked) {
                subActorGroup->setVisible(checked);
                aliasGroup->setVisible(checked);
            });

    mainLayout->addWidget(subActorGroup);
    mainLayout->addWidget(aliasGroup);
    mainLayout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) return false;

    def.name        = nameEdit->text().trimmed().toStdString();
    def.alias       = aliasEdit->text().trimmed().toStdString();
    def.field       = fieldEdit->text().trimmed().toStdString();
    def.pattern     = patternEdit->text().trimmed().toStdString();
    def.enabled     = enabledBox->isChecked();
    def.isSelf      = selfBox->isChecked();
    def.useCaptures = captureBox->isChecked();
    // Read directedTo: prefer the editable text; treat "(none)" as empty
    {
        const QString dt = directedToCombo->currentText().trimmed();
        def.directedTo = (dt == tr("(none)")) ? std::string{} : dt.toStdString();
    }
    // When marking as self, clear isSelf on all other definitions
    if (def.isSelf)
        for (auto& d : m_definitions)
            if (d.name != def.name) d.isSelf = false;

    // Read subactor directed-to and capture aliases tables
    if (def.useCaptures)
    {
        def.subActorDirectedTo.clear();
        for (int r = 0; r < subActorTable->rowCount(); ++r)
        {
            const QTableWidgetItem* ni = subActorTable->item(r, 0);
            const QTableWidgetItem* ti = subActorTable->item(r, 1);
            if (!ni || !ti) continue;
            const std::string k = ni->text().trimmed().toStdString();
            const std::string v = ti->text().trimmed().toStdString();
            if (!k.empty() && !v.empty())
                def.subActorDirectedTo[k] = v;
        }
        def.captureAliases.clear();
        for (int r = 0; r < aliasTable->rowCount(); ++r)
        {
            const QTableWidgetItem* ki = aliasTable->item(r, 0);
            const QTableWidgetItem* vi = aliasTable->item(r, 1);
            if (!ki || !vi) continue;
            const std::string k = ki->text().trimmed().toStdString();
            const std::string v = vi->text().trimmed().toStdString();
            if (!k.empty() && !v.empty())
                def.captureAliases[k] = v;
        }
    }
    return true;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void ActorDefinitionsPanel::UpdateActorDirection(
    const QString& defName,
    const QString& actorName,
    bool           isSubActor,
    const QString& target)
{
    util::Logger::Info("[ActorDefinitionsPanel] UpdateActorDirection: def='{}' actor='{}' "
                       "isSubActor={} target='{}'",
                       defName.toStdString(), actorName.toStdString(),
                       isSubActor, target.toStdString());
    for (auto& def : m_definitions)
    {
        if (def.name != defName.toStdString()) continue;

        if (isSubActor)
        {
            if (target.isEmpty())
                def.subActorDirectedTo.erase(actorName.toStdString());
            else
                def.subActorDirectedTo[actorName.toStdString()] = target.toStdString();
        }
        else
        {
            // Update ALL definitions sharing this actor name so that "Set Directed To"
            // from the context menu applies consistently across all matching patterns.
            def.directedTo = target.toStdString();
        }
    }
    RebuildTable();
    EmitAndSave();
}

void ActorDefinitionsPanel::EmitAndSave()
{
    emit DefinitionsChanged(m_definitions);

    // Always auto-save to the default path so that actors.json is kept
    // up-to-date regardless of any "Save As" export location.
    const std::string autoSavePath = DefaultFilePath();
    try
    {
        std::filesystem::create_directories(
            std::filesystem::path(autoSavePath).parent_path());

        std::ofstream ofs(autoSavePath);
        if (!ofs)
        {
            util::Logger::Error("[ActorDefinitionsPanel] Auto-save: cannot open '{}'",
                                autoSavePath);
            SetStatus(tr("Auto-save failed: cannot open %1")
                .arg(QString::fromStdString(autoSavePath)), true);
            return;
        }
        const std::string json = ActorDefinition::ListToJson(m_definitions).dump(2);
        ofs << json;
        ofs.flush();
        if (!ofs)
        {
            util::Logger::Error("[ActorDefinitionsPanel] Auto-save: write error for '{}'",
                                autoSavePath);
            SetStatus(tr("Auto-save failed: write error"), true);
        }
        else
        {
            util::Logger::Debug("[ActorDefinitionsPanel] Auto-saved {} definition(s) to '{}'",
                                m_definitions.size(), autoSavePath);
            SetStatus(tr("Auto-saved %1 definition(s) → %2")
                .arg(m_definitions.size())
                .arg(QString::fromStdString(autoSavePath)), false);
        }
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[ActorDefinitionsPanel] Auto-save exception for '{}': {}",
                            autoSavePath, e.what());
        SetStatus(tr("Auto-save error: %1").arg(QString::fromStdString(e.what())), true);
    }
}

std::string ActorDefinitionsPanel::DefaultFilePath()
{
    return (config::GetConfig().GetDefaultAppPath() / "actors.json").string();
}

void ActorDefinitionsPanel::SetStatus(const QString& msg, bool isError)
{
    if (!m_statusLabel) return;
    m_statusLabel->setText(msg);
    // For errors use red; for success/info inherit the palette text colour so
    // it remains readable on both light and dark themes.
    if (isError)
        m_statusLabel->setStyleSheet("font-style: italic; color: #c0392b;");
    else
        m_statusLabel->setStyleSheet("font-style: italic;");
}

} // namespace ui::qt
