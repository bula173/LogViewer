#include "ActorDefinitionsPanel.hpp"

#include "Config.hpp"

#include <nlohmann/json.hpp>

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
                SetStatus(tr("Auto-loaded %1 definition(s)").arg(m_definitions.size()), false);
        }
        catch (const std::exception& e)
        {
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
    m_table = new QTableWidget(0, 5, this);
    m_table->setHorizontalHeaderLabels(
        {tr("On"), tr("Name"), tr("Field"), tr("Pattern (regexp)"), tr("Captures")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setToolTip(tr(
        "Captures: when ✓, each () group in the pattern produces a separate actor"));
    m_table->setColumnWidth(1, 110);
    m_table->setColumnWidth(2, 90);
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
    auto* btnRow2   = new QHBoxLayout();
    auto* saveBtn   = new QPushButton(tr("Save"),      this);
    auto* saveAsBtn = new QPushButton(tr("Save As…"),  this);
    auto* loadBtn   = new QPushButton(tr("Load…"),     this);
    btnRow2->addWidget(saveBtn);
    btnRow2->addWidget(saveAsBtn);
    btnRow2->addWidget(loadBtn);
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

        auto* onItem = new QTableWidgetItem();
        onItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        onItem->setCheckState(def.enabled ? Qt::Checked : Qt::Unchecked);
        m_table->setItem(row, 0, onItem);

        m_table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(def.name)));
        m_table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(def.field)));
        m_table->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(def.pattern)));

        auto* capItem = new QTableWidgetItem(def.useCaptures ? tr("\u2713") : QString());
        capItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
        capItem->setTextAlignment(Qt::AlignCenter);
        capItem->setToolTip(def.useCaptures
            ? tr("Each capture group () produces a separate actor")
            : tr("Actor name is fixed (no capture groups used)"));
        m_table->setItem(row, 4, capItem);

        if (!def.enabled)
        {
            for (int c = 1; c < 5; ++c)
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
        emit DefinitionsChanged(m_definitions);
        SetStatus(tr("Saved %1 definition(s) to %2.")
            .arg(m_definitions.size())
            .arg(QString::fromStdString(m_currentFilePath)), false);
    }
    catch (const std::exception& e)
    {
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
            SetStatus(tr("File loaded but contains 0 definitions."), true);
        else
            SetStatus(tr("Loaded %1 definition(s) from file.").arg(m_definitions.size()), false);

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
        QMessageBox::warning(this, tr("Load Error"),
            tr("Could not load actors: %1").arg(QString::fromStdString(e.what())));
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
    m_definitions[static_cast<size_t>(row)].enabled = on;

    // Update row text colour to reflect enabled state
    {
        m_rebuilding = true;
        for (int c = 1; c < 5; ++c)
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
    auto* fieldEdit   = new QLineEdit(QString::fromStdString(def.field),   &dlg);
    auto* patternEdit = new QLineEdit(QString::fromStdString(def.pattern), &dlg);
    auto* enabledBox  = new QCheckBox(tr("Enabled"), &dlg);
    enabledBox->setChecked(def.enabled);
    auto* captureBox  = new QCheckBox(tr("Use capture groups as actors"), &dlg);
    captureBox->setChecked(def.useCaptures);
    captureBox->setToolTip(tr(
        "When checked, each () group in the pattern produces a separate actor "
        "named after the captured text. Requires at least one capture group."));

    fieldEdit->setPlaceholderText(tr("(empty = any field)"));
    patternEdit->setPlaceholderText(tr("Regular expression …"));

    auto* statusLabel = new QLabel(&dlg);
    statusLabel->setWordWrap(true);

    form->addRow(tr("Name:"),    nameEdit);
    form->addRow(tr("Field:"),   fieldEdit);
    form->addRow(tr("Pattern:"), patternEdit);
    form->addRow(QString(),      captureBox);
    form->addRow(QString(),      enabledBox);
    form->addRow(QString(),      statusLabel);

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
    connect(captureBox, &QCheckBox::stateChanged, &dlg, [&](int) { validate(); });
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
    mainLayout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) return false;

    def.name        = nameEdit->text().trimmed().toStdString();
    def.field       = fieldEdit->text().trimmed().toStdString();
    def.pattern     = patternEdit->text().trimmed().toStdString();
    def.enabled     = enabledBox->isChecked();
    def.useCaptures = captureBox->isChecked();
    return true;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

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
            SetStatus(tr("Auto-save failed: cannot open %1")
                .arg(QString::fromStdString(autoSavePath)), true);
            return;
        }
        const std::string json = ActorDefinition::ListToJson(m_definitions).dump(2);
        ofs << json;
        ofs.flush();
        if (!ofs)
            SetStatus(tr("Auto-save failed: write error"), true);
        else
            SetStatus(tr("Auto-saved %1 definition(s) → %2")
                .arg(m_definitions.size())
                .arg(QString::fromStdString(autoSavePath)), false);
    }
    catch (const std::exception& e)
    {
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
