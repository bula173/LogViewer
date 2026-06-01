#include "ScenariosPanel.hpp"

#include "Logger.hpp"
#include "utils/PanelUtils.hpp"

#include <QButtonGroup>
#include <QCheckBox>
#include <QDialogButtonBox>
#include <QGuiApplication>
#include <QClipboard>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QInputDialog>
#include <QLabel>
#include <QMessageBox>
#include <QPlainTextEdit>
#include <QRadioButton>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <algorithm>
#include <map>
#include <set>
#include <sstream>
#include <string>

namespace ui::qt {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

ScenariosPanel::ScenariosPanel(db::EventsContainer& events,
                               EventsTableView*     eventsView,
                               QWidget*             parent)
    : QWidget(parent), m_events(events), m_eventsView(eventsView)
{
    BuildLayout();
    LoadFromSettings();
    if (m_scenarios.empty())
        UpdateScenarioCombo(-1);
}

void ScenariosPanel::BuildLayout()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // ── Scenario selector row ─────────────────────────────────────────────
    auto* topRow = new QHBoxLayout();
    topRow->addWidget(new QLabel(tr("Scenario:"), this));
    m_scenarioCombo = new QComboBox(this);
    m_scenarioCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    m_scenarioCombo->setToolTip(tr("Select the active scenario"));
    topRow->addWidget(m_scenarioCombo, 1);

    m_newBtn = new QPushButton(tr("New"), this);
    m_newBtn->setToolTip(tr("Create a new scenario"));
    topRow->addWidget(m_newBtn);

    m_renameBtn = new QPushButton(tr("Rename"), this);
    m_renameBtn->setToolTip(tr("Rename the active scenario"));
    topRow->addWidget(m_renameBtn);

    m_deleteBtn = new QPushButton(tr("Delete"), this);
    m_deleteBtn->setToolTip(tr("Delete the active scenario"));
    topRow->addWidget(m_deleteBtn);
    layout->addLayout(topRow);

    // ── Events table ──────────────────────────────────────────────────────
    m_eventsTable = new QTableWidget(0, 3, this);
    m_eventsTable->setHorizontalHeaderLabels(
        {tr("Row"), tr("Timestamp"), tr("Summary")});
    m_eventsTable->horizontalHeader()->setStretchLastSection(true);
    m_eventsTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_eventsTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_eventsTable->setColumnWidth(1, 160);
    m_eventsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_eventsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_eventsTable->setSelectionMode(QAbstractItemView::SingleSelection);
    m_eventsTable->setAlternatingRowColors(true);
    m_eventsTable->setToolTip(tr("Events in this scenario — double-click to jump to event"));
    layout->addWidget(m_eventsTable, 1);

    // ── Action buttons ────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    m_addBtn = new QPushButton(tr("Add Selected Event"), this);
    m_addBtn->setToolTip(tr("Add the currently selected event from the Events tab to this scenario"));
    btnRow->addWidget(m_addBtn);

    m_removeBtn = new QPushButton(tr("Remove"), this);
    m_removeBtn->setToolTip(tr("Remove the selected event from this scenario"));
    btnRow->addWidget(m_removeBtn);

    m_upBtn = new QPushButton(tr("▲"), this);
    m_upBtn->setToolTip(tr("Move event up"));
    m_upBtn->setFixedWidth(32);
    btnRow->addWidget(m_upBtn);

    m_downBtn = new QPushButton(tr("▼"), this);
    m_downBtn->setToolTip(tr("Move event down"));
    m_downBtn->setFixedWidth(32);
    btnRow->addWidget(m_downBtn);

    btnRow->addStretch();

    m_exportBtn = new QPushButton(tr("Export…"), this);
    m_exportBtn->setToolTip(tr("Export this scenario as text, Markdown table, or JSON Lines"));
    btnRow->addWidget(m_exportBtn);
    layout->addLayout(btnRow);

    // ── Status label ──────────────────────────────────────────────────────
    m_statusLabel = new QLabel(tr("No scenario selected"), this);
    m_statusLabel->setAlignment(Qt::AlignRight);
    layout->addWidget(m_statusLabel);

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_scenarioCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ScenariosPanel::OnScenarioChanged);

    connect(m_newBtn,    &QPushButton::clicked, this, &ScenariosPanel::OnNewScenario);
    connect(m_renameBtn, &QPushButton::clicked, this, &ScenariosPanel::OnRenameScenario);
    connect(m_deleteBtn, &QPushButton::clicked, this, &ScenariosPanel::OnDeleteScenario);
    connect(m_addBtn,    &QPushButton::clicked, this, &ScenariosPanel::OnAddSelectedEvent);
    connect(m_removeBtn, &QPushButton::clicked, this, &ScenariosPanel::OnRemoveEvent);
    connect(m_upBtn,     &QPushButton::clicked, this, &ScenariosPanel::OnMoveUp);
    connect(m_downBtn,   &QPushButton::clicked, this, &ScenariosPanel::OnMoveDown);
    connect(m_exportBtn, &QPushButton::clicked, this, &ScenariosPanel::OnExport);

    connect(m_eventsTable, &QTableWidget::itemSelectionChanged,
            this, &ScenariosPanel::UpdateButtonStates);

    connect(m_eventsTable, &QTableWidget::itemDoubleClicked,
            this, [this](QTableWidgetItem* item) {
                if (!item) return;
                const Scenario* sc = ActiveScenario();
                if (!sc) return;
                const int row = m_eventsTable->row(item);
                if (row < 0 || row >= static_cast<int>(sc->events.size())) return;
                m_eventsView->ScrollToActualRow(sc->events[static_cast<size_t>(row)].row);
            });
}

// ---------------------------------------------------------------------------
// Public slot
// ---------------------------------------------------------------------------

void ScenariosPanel::AddEventFromRow(int actualRow)
{
    if (actualRow < 0 || actualRow >= static_cast<int>(m_events.Size()))
        return;

    // Auto-create a default scenario if none exists
    if (m_scenarios.empty())
    {
        util::Logger::Debug("[ScenariosPanel] No scenarios exist — creating default 'Scenario 1'");
        m_scenarios.push_back({"Scenario 1", {}});
        UpdateScenarioCombo(0);
    }

    Scenario* sc = ActiveScenario();
    if (!sc) return;

    ScenarioEvent se;
    se.row = actualRow;
    FillEventStrings(se);
    sc->events.push_back(std::move(se));

    util::Logger::Info("[ScenariosPanel] Added event row {} to scenario '{}' ({} event(s) total)",
                       actualRow, sc->name, sc->events.size());
    RebuildEventsTable();
    SaveToSettings();
    m_statusLabel->setText(
        tr("Added event #%1 to \"%2\"")
            .arg(actualRow)
            .arg(QString::fromStdString(sc->name)));
}

// ---------------------------------------------------------------------------
// Private slots
// ---------------------------------------------------------------------------

void ScenariosPanel::OnNewScenario()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("New Scenario"), tr("Scenario name:"),
        QLineEdit::Normal,
        tr("Scenario %1").arg(m_scenarios.size() + 1), &ok);
    if (!ok) return;
    if (name.trimmed().isEmpty())
    {
        util::Logger::Warn("[ScenariosPanel] OnNewScenario: empty scenario name entered — ignoring");
        return;
    }

    util::Logger::Debug("[ScenariosPanel] OnNewScenario: user entered name '{}'", name.trimmed().toStdString());
    m_scenarios.push_back({name.trimmed().toStdString(), {}});
    UpdateScenarioCombo(static_cast<int>(m_scenarios.size()) - 1);
    SaveToSettings();
    util::Logger::Info("[ScenariosPanel] Scenario '{}' created", name.trimmed().toStdString());
}

void ScenariosPanel::OnRenameScenario()
{
    Scenario* sc = ActiveScenario();
    if (!sc) return;

    const std::string oldName = sc->name;
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, tr("Rename Scenario"), tr("New name:"),
        QLineEdit::Normal,
        QString::fromStdString(sc->name), &ok);
    if (!ok) return;
    if (name.trimmed().isEmpty())
    {
        util::Logger::Warn("[ScenariosPanel] OnRenameScenario: empty name entered — ignoring");
        return;
    }

    util::Logger::Debug("[ScenariosPanel] OnRenameScenario: '{}' -> '{}'", oldName, name.trimmed().toStdString());
    sc->name = name.trimmed().toStdString();
    const int idx = m_scenarioCombo->currentIndex();
    m_scenarioCombo->blockSignals(true);
    m_scenarioCombo->setItemText(idx, QString::fromStdString(sc->name));
    m_scenarioCombo->blockSignals(false);
    m_statusLabel->setText(
        tr("\"%1\" — %2 event(s)")
            .arg(QString::fromStdString(sc->name))
            .arg(sc->events.size()));
    SaveToSettings();
    util::Logger::Info("[ScenariosPanel] Scenario renamed '{}' -> '{}'", oldName, sc->name);
}

void ScenariosPanel::OnDeleteScenario()
{
    Scenario* sc = ActiveScenario();
    if (!sc) return;

    util::Logger::Debug("[ScenariosPanel] OnDeleteScenario: '{}' ({} event(s))", sc->name, sc->events.size());

    const auto answer = QMessageBox::question(
        this, tr("Delete Scenario"),
        tr("Delete scenario \"%1\" and its %2 event(s)?")
            .arg(QString::fromStdString(sc->name))
            .arg(sc->events.size()),
        QMessageBox::Yes | QMessageBox::No);
    if (answer != QMessageBox::Yes) return;

    const std::string deletedName  = sc->name;
    const size_t      deletedCount = sc->events.size();
    m_scenarios.erase(m_scenarios.begin() + m_activeScenario);
    const int newIdx = m_scenarios.empty() ? -1 :
        std::min(m_activeScenario, static_cast<int>(m_scenarios.size()) - 1);
    m_activeScenario = -1; // prevent OnScenarioChanged from using stale index
    UpdateScenarioCombo(newIdx);
    SaveToSettings();
    util::Logger::Info("[ScenariosPanel] Scenario '{}' deleted ({} events)", deletedName, deletedCount);
}

void ScenariosPanel::OnScenarioChanged(int comboIndex)
{
    m_activeScenario = comboIndex;
    RebuildEventsTable();
    UpdateButtonStates();

    const Scenario* sc = ActiveScenario();
    if (sc)
        util::Logger::Debug("[ScenariosPanel] OnScenarioChanged: switched to '{}' ({} event(s))", sc->name, sc->events.size());
    else
        util::Logger::Debug("[ScenariosPanel] OnScenarioChanged: no scenario selected (index {})", comboIndex);

    m_statusLabel->setText(sc
        ? tr("\"%1\" — %2 event(s)").arg(QString::fromStdString(sc->name)).arg(sc->events.size())
        : tr("No scenario selected"));
}

void ScenariosPanel::OnAddSelectedEvent()
{
    AddEventFromRow(m_eventsView->CurrentActualRow());
}

void ScenariosPanel::OnRemoveEvent()
{
    Scenario* sc = ActiveScenario();
    if (!sc) return;
    const int row = SelectedEventRow();
    if (row < 0 || row >= static_cast<int>(sc->events.size())) return;

    const int removedActualRow = sc->events[static_cast<size_t>(row)].row;
    util::Logger::Debug("[ScenariosPanel] OnRemoveEvent: removing event at table row {} (actual row {}) from scenario '{}'",
                        row, removedActualRow, sc->name);
    sc->events.erase(sc->events.begin() + row);
    RebuildEventsTable();
    SaveToSettings();
    m_statusLabel->setText(
        tr("\"%1\" — %2 event(s)")
            .arg(QString::fromStdString(sc->name))
            .arg(sc->events.size()));
}

void ScenariosPanel::OnMoveUp()
{
    Scenario* sc = ActiveScenario();
    if (!sc) return;
    const int row = SelectedEventRow();
    if (row <= 0 || row >= static_cast<int>(sc->events.size())) return;

    util::Logger::Debug("[ScenariosPanel] OnMoveUp: moving event at row {} up in scenario '{}'", row, sc->name);
    std::swap(sc->events[static_cast<size_t>(row)],
              sc->events[static_cast<size_t>(row) - 1]);
    RebuildEventsTable();
    SaveToSettings();
    m_eventsTable->selectRow(row - 1);
}

void ScenariosPanel::OnMoveDown()
{
    Scenario* sc = ActiveScenario();
    if (!sc) return;
    const int row = SelectedEventRow();
    if (row < 0 || row + 1 >= static_cast<int>(sc->events.size())) return;

    util::Logger::Debug("[ScenariosPanel] OnMoveDown: moving event at row {} down in scenario '{}'", row, sc->name);
    std::swap(sc->events[static_cast<size_t>(row)],
              sc->events[static_cast<size_t>(row) + 1]);
    RebuildEventsTable();
    SaveToSettings();
    m_eventsTable->selectRow(row + 1);
}

// ---------------------------------------------------------------------------
// Export dialog
// ---------------------------------------------------------------------------

void ScenariosPanel::OnExport()
{
    const Scenario* sc = ActiveScenario();
    if (!sc || sc->events.empty())
    {
        util::Logger::Warn("[ScenariosPanel] OnExport: no events in active scenario — export aborted");
        QMessageBox::information(this, tr("Export Scenario"),
            tr("The active scenario has no events to export."));
        return;
    }

    util::Logger::Debug("[ScenariosPanel] OnExport: starting export for scenario '{}' ({} event(s))",
                        sc->name, sc->events.size());

    // ── 1. Collect all field names across scenario events ─────────────────
    std::vector<std::string> allFields;
    std::set<std::string> fieldSet;
    for (const auto& se : sc->events)
    {
        if (se.row < 0 || se.row >= static_cast<int>(m_events.Size())) continue;
        for (const auto& [k, v] : m_events.GetEvent(static_cast<size_t>(se.row)).getEventItems())
            if (fieldSet.insert(k).second)
                allFields.push_back(k);
    }
    if (allFields.empty())
    {
        QMessageBox::information(this, tr("Export Scenario"),
            tr("No fields found in the scenario events."));
        return;
    }

    // Common fields to pre-check by default
    static const std::set<std::string> kDefaultChecked{
        "timestamp", "time", "datetime", "@timestamp", "date",
        "level", "severity", "type", "message", "msg", "text"};

    // ── 2. Build export dialog ────────────────────────────────────────────
    auto* dlg = new QDialog(this);
    dlg->setWindowTitle(
        tr("Export Scenario: %1").arg(QString::fromStdString(sc->name)));
    dlg->setAttribute(Qt::WA_DeleteOnClose);
    dlg->resize(680, 560);

    auto* mainLayout = new QVBoxLayout(dlg);
    mainLayout->setContentsMargins(8, 8, 8, 8);
    mainLayout->setSpacing(6);

    auto* splitter = new QSplitter(Qt::Vertical, dlg);
    mainLayout->addWidget(splitter, 1);

    // ── Top half: field checkboxes ────────────────────────────────────────
    auto* topWidget = new QWidget(splitter);
    auto* topLayout = new QVBoxLayout(topWidget);
    topLayout->setContentsMargins(0, 0, 0, 0);

    auto* fieldsBox = new QGroupBox(tr("Fields to include:"), topWidget);
    auto* fieldsLayout = new QVBoxLayout(fieldsBox);

    // Select-all / select-none helpers
    auto* selRow = new QHBoxLayout();
    auto* selAllBtn  = new QPushButton(tr("Select All"),  fieldsBox);
    auto* selNoneBtn = new QPushButton(tr("Select None"), fieldsBox);
    selRow->addWidget(selAllBtn);
    selRow->addWidget(selNoneBtn);
    selRow->addStretch();
    fieldsLayout->addLayout(selRow);

    auto* scrollArea = new QScrollArea(fieldsBox);
    scrollArea->setWidgetResizable(true);
    auto* checkContainer = new QWidget(scrollArea);
    auto* checkLayout = new QVBoxLayout(checkContainer);
    checkLayout->setContentsMargins(4, 4, 4, 4);
    checkLayout->setSpacing(2);

    std::vector<QCheckBox*> checkBoxes;
    checkBoxes.reserve(allFields.size());
    for (const auto& field : allFields)
    {
        auto* cb = new QCheckBox(QString::fromStdString(field), checkContainer);
        cb->setChecked(kDefaultChecked.count(field) > 0);
        checkLayout->addWidget(cb);
        checkBoxes.push_back(cb);
    }
    checkLayout->addStretch();
    checkContainer->setLayout(checkLayout);
    scrollArea->setWidget(checkContainer);
    scrollArea->setMinimumHeight(120);
    fieldsLayout->addWidget(scrollArea);
    topLayout->addWidget(fieldsBox);

    // ── Format options ────────────────────────────────────────────────────
    auto* fmtBox    = new QGroupBox(tr("Format:"), topWidget);
    auto* fmtLayout = new QHBoxLayout(fmtBox);
    auto* fmtGroup  = new QButtonGroup(fmtBox);
    auto* rbPlain   = new QRadioButton(tr("Plain text"), fmtBox);
    auto* rbMd      = new QRadioButton(tr("Markdown table"), fmtBox);
    auto* rbJson    = new QRadioButton(tr("JSON Lines"), fmtBox);
    rbMd->setChecked(true);
    fmtGroup->addButton(rbPlain, 0);
    fmtGroup->addButton(rbMd,    1);
    fmtGroup->addButton(rbJson,  2);
    fmtLayout->addWidget(rbPlain);
    fmtLayout->addWidget(rbMd);
    fmtLayout->addWidget(rbJson);
    fmtLayout->addStretch();
    topLayout->addWidget(fmtBox);

    splitter->addWidget(topWidget);

    // ── Bottom half: preview ──────────────────────────────────────────────
    auto* previewBox    = new QGroupBox(tr("Preview:"), splitter);
    auto* previewLayout = new QVBoxLayout(previewBox);
    auto* previewEdit   = new QPlainTextEdit(previewBox);
    previewEdit->setReadOnly(true);
    previewEdit->setLineWrapMode(QPlainTextEdit::NoWrap);
    {
        QFont f = previewEdit->font();
        f.setFamily("Courier New, Monospace");
        previewEdit->setFont(f);
    }
    previewLayout->addWidget(previewEdit);
    splitter->addWidget(previewBox);
    splitter->setSizes({280, 220});

    // ── Button row ────────────────────────────────────────────────────────
    auto* btnRow = new QHBoxLayout();
    auto* genBtn  = new QPushButton(tr("Generate Preview"), dlg);
    auto* copyBtn = new QPushButton(tr("Copy to Clipboard"), dlg);
    auto* closeBtn = new QPushButton(tr("Close"), dlg);
    genBtn->setDefault(true);
    btnRow->addWidget(genBtn);
    btnRow->addWidget(copyBtn);
    btnRow->addStretch();
    btnRow->addWidget(closeBtn);
    mainLayout->addLayout(btnRow);

    // ── Select-all / none helpers ─────────────────────────────────────────
    connect(selAllBtn,  &QPushButton::clicked, dlg, [&checkBoxes]() {
        for (auto* cb : checkBoxes) cb->setChecked(true);
    });
    connect(selNoneBtn, &QPushButton::clicked, dlg, [&checkBoxes]() {
        for (auto* cb : checkBoxes) cb->setChecked(false);
    });

    // ── Generate function ─────────────────────────────────────────────────
    const std::vector<ScenarioEvent> scenarioEvents = sc->events;

    auto generate = [this, &allFields, &checkBoxes, rbPlain, rbJson,
                     scenarioEvents, previewEdit, sc]()
    {
        // Collect selected fields in declaration order
        std::vector<std::string> selected;
        for (size_t i = 0; i < checkBoxes.size(); ++i)
            if (checkBoxes[i]->isChecked())
                selected.push_back(allFields[i]);

        if (selected.empty())
        {
            util::Logger::Warn("[ScenariosPanel] OnExport: no fields selected for export");
            previewEdit->setPlainText(tr("// Select at least one field."));
            return;
        }

        const int fmt = rbPlain->isChecked() ? 0 : rbJson->isChecked() ? 2 : 1;

        QString out;

        if (fmt == 1) // Markdown table
        {
            // Header
            QString header = "|";
            QString divider = "|";
            for (const auto& f : selected)
            {
                header  += QString(" %1 |").arg(QString::fromStdString(f));
                divider += " --- |";
            }
            out += header  + "\n";
            out += divider + "\n";
        }

        for (const auto& se : scenarioEvents)
        {
            if (se.row < 0 || se.row >= static_cast<int>(m_events.Size()))
                continue;

            const db::LogEvent& ev = m_events.GetEvent(static_cast<size_t>(se.row));

            if (fmt == 0) // Plain text
            {
                QStringList parts;
                for (const auto& f : selected)
                {
                    const std::string val = ev.findByKey(f);
                    parts << QString("%1: %2")
                                 .arg(QString::fromStdString(f))
                                 .arg(QString::fromStdString(val));
                }
                out += parts.join(" | ") + "\n";
            }
            else if (fmt == 1) // Markdown
            {
                QString row = "|";
                for (const auto& f : selected)
                {
                    std::string val = ev.findByKey(f);
                    // Escape pipe characters in value
                    QString qval = QString::fromStdString(val);
                    qval.replace('|', "\\|");
                    qval.replace('\n', ' ');
                    row += QString(" %1 |").arg(qval);
                }
                out += row + "\n";
            }
            else // JSON Lines
            {
                nlohmann::json obj;
                for (const auto& f : selected)
                    obj[f] = ev.findByKey(f);
                out += QString::fromStdString(obj.dump()) + '\n';
            }
        }

        if (fmt == 1)
        {
            out += "\n";
            out += QString("*Scenario: %1 — %2 event(s)*\n")
                       .arg(QString::fromStdString(sc->name))
                       .arg(scenarioEvents.size());
        }

        previewEdit->setPlainText(out);
        util::Logger::Info("[ScenariosPanel] Exported {} rows to preview (scenario '{}', {} field(s), format {})",
                           scenarioEvents.size(), sc->name, selected.size(), fmt);
    };

    connect(genBtn,  &QPushButton::clicked, dlg, generate);
    connect(copyBtn, &QPushButton::clicked, dlg, [previewEdit]() {
        QGuiApplication::clipboard()->setText(previewEdit->toPlainText());
    });
    connect(closeBtn, &QPushButton::clicked, dlg, &QDialog::close);

    // Auto-generate on open
    generate();

    dlg->exec();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void ScenariosPanel::RebuildEventsTable()
{
    const Scenario* sc = ActiveScenario();
    const int count = sc ? static_cast<int>(sc->events.size()) : 0;

    m_eventsTable->setRowCount(count);
    for (int i = 0; i < count; ++i)
    {
        const auto& se = sc->events[static_cast<size_t>(i)];
        m_eventsTable->setItem(i, 0, new QTableWidgetItem(QString::number(se.row)));
        m_eventsTable->setItem(i, 1,
            new QTableWidgetItem(QString::fromStdString(se.timestamp)));
        m_eventsTable->setItem(i, 2,
            new QTableWidgetItem(QString::fromStdString(se.summary)));
    }
    UpdateButtonStates();
}

void ScenariosPanel::UpdateScenarioCombo(int selectIndex)
{
    m_scenarioCombo->blockSignals(true);
    m_scenarioCombo->clear();
    for (const auto& sc : m_scenarios)
        m_scenarioCombo->addItem(QString::fromStdString(sc.name));
    if (selectIndex >= 0 && selectIndex < static_cast<int>(m_scenarios.size()))
        m_scenarioCombo->setCurrentIndex(selectIndex);
    else
        m_scenarioCombo->setCurrentIndex(-1);
    m_scenarioCombo->blockSignals(false);

    OnScenarioChanged(m_scenarioCombo->currentIndex());
}

void ScenariosPanel::UpdateButtonStates()
{
    const Scenario* sc  = ActiveScenario();
    const bool      has = sc != nullptr;
    const int       sel = SelectedEventRow();
    const int       cnt = sc ? static_cast<int>(sc->events.size()) : 0;

    m_renameBtn->setEnabled(has);
    m_deleteBtn->setEnabled(has);
    m_addBtn->setEnabled(has);
    m_exportBtn->setEnabled(has && cnt > 0);
    m_removeBtn->setEnabled(has && sel >= 0);
    m_upBtn->setEnabled(has && sel > 0);
    m_downBtn->setEnabled(has && sel >= 0 && sel < cnt - 1);
}

void ScenariosPanel::FillEventStrings(ScenarioEvent& se) const
{
    if (se.row < 0 || se.row >= static_cast<int>(m_events.Size())) return;

    const db::LogEvent& ev = m_events.GetEvent(static_cast<size_t>(se.row));

    for (const auto& f : panel_utils::kTsFields)
    {
        se.timestamp = ev.findByKey(f);
        if (!se.timestamp.empty()) break;
    }
    for (const auto& f : panel_utils::kMsgFields)
    {
        se.summary = ev.findByKey(f);
        if (!se.summary.empty()) break;
    }
    if (se.summary.empty())
    {
        const auto& items = ev.getEventItems();
        if (!items.empty()) se.summary = items.front().second;
    }
    if (se.summary.size() > 80)
        se.summary = se.summary.substr(0, 77) + "...";
}

nlohmann::json ScenariosPanel::GetSessionData() const
{
    nlohmann::json arr = nlohmann::json::array();
    for (const auto& sc : m_scenarios)
    {
        nlohmann::json events = nlohmann::json::array();
        for (const auto& se : sc.events)
            events.push_back({{"row", se.row},
                              {"timestamp", se.timestamp},
                              {"summary",   se.summary}});
        arr.push_back({{"name", sc.name}, {"events", std::move(events)}});
    }
    return arr;
}

void ScenariosPanel::LoadSessionData(const nlohmann::json& data)
{
    m_scenarios.clear();
    for (const auto& item : data)
    {
        Scenario sc;
        sc.name = item.value("name", std::string{});
        if (item.contains("events"))
        {
            for (const auto& ev : item["events"])
            {
                ScenarioEvent se;
                se.row       = ev.value("row",       -1);
                se.timestamp = ev.value("timestamp", std::string{});
                se.summary   = ev.value("summary",   std::string{});
                if (se.row >= 0)
                    sc.events.push_back(std::move(se));
            }
        }
        if (!sc.name.empty())
            m_scenarios.push_back(std::move(sc));
    }
    UpdateScenarioCombo(m_scenarios.empty() ? -1 : 0);
}

void ScenariosPanel::SaveToSettings() const
{
    QSettings s("LogViewer", "LogViewer");
    s.setValue("scenarios/data",
               QString::fromStdString(GetSessionData().dump()));
}

void ScenariosPanel::LoadFromSettings()
{
    QSettings s("LogViewer", "LogViewer");
    const QString raw = s.value("scenarios/data").toString();
    if (raw.isEmpty()) return;
    try {
        LoadSessionData(nlohmann::json::parse(raw.toStdString()));
    } catch (const std::exception& ex) {
        util::Logger::Warn("[ScenariosPanel] Failed to load scenarios from settings: {}", ex.what());
    }
}

int ScenariosPanel::SelectedEventRow() const
{
    const auto sel = m_eventsTable->selectedItems();
    if (sel.isEmpty()) return -1;
    return m_eventsTable->row(sel.first());
}

ScenariosPanel::Scenario* ScenariosPanel::ActiveScenario()
{
    if (m_activeScenario < 0 ||
        m_activeScenario >= static_cast<int>(m_scenarios.size()))
        return nullptr;
    return &m_scenarios[static_cast<size_t>(m_activeScenario)];
}

} // namespace ui::qt
