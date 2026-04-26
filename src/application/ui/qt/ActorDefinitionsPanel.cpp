#include "ActorDefinitionsPanel.hpp"

#include "Config.hpp"

#include <nlohmann/json.hpp>

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>

#include <fstream>

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
        }
        catch (...) {}
    }
}

void ActorDefinitionsPanel::BuildLayout()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    auto* infoLabel = new QLabel(
        tr("Define actors by name and a regular expression.\n"
           "Each matching event will be attributed to that actor."),
        this);
    infoLabel->setWordWrap(true);
    layout->addWidget(infoLabel);

    // ── Table ─────────────────────────────────────────────────────────────
    m_table = new QTableWidget(0, 4, this);
    m_table->setHorizontalHeaderLabels(
        {tr("On"), tr("Name"), tr("Field"), tr("Pattern (regexp)")});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::Interactive);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->setColumnWidth(1, 110);
    m_table->setColumnWidth(2, 90);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->verticalHeader()->hide();
    m_table->setAlternatingRowColors(true);
    layout->addWidget(m_table, 1);

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
}

// ---------------------------------------------------------------------------
// Table population
// ---------------------------------------------------------------------------

void ActorDefinitionsPanel::RebuildTable()
{
    const QSignalBlocker blocker(m_table); // prevent itemChanged during rebuild
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

        if (!def.enabled)
        {
            for (int c = 1; c < 4; ++c)
                if (auto* it = m_table->item(row, c))
                    it->setForeground(QColor(150, 150, 150));
        }
    }
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
        std::ofstream ofs(m_currentFilePath);
        if (!ofs) throw std::runtime_error("Cannot open file for writing");
        ofs << ActorDefinition::ListToJson(m_definitions).dump(2);
        emit DefinitionsChanged(m_definitions);
    }
    catch (const std::exception& e)
    {
        QMessageBox::warning(this, tr("Save Error"),
            tr("Could not save actors: %1").arg(QString::fromStdString(e.what())));
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
    dlg.selectFile("actors.json");

    if (dlg.exec() != QDialog::Accepted) return;
    const QString path = dlg.selectedFiles().value(0);
    if (path.isEmpty()) return;

    m_currentFilePath = path.toStdString();
    HandleSave();
}

void ActorDefinitionsPanel::HandleLoad()
{
    QFileDialog dlg(this, tr("Load Actor Definitions"));
    dlg.setNameFilter(tr("Actor Definitions (*.json)"));
    dlg.setAcceptMode(QFileDialog::AcceptOpen);
    dlg.setFileMode(QFileDialog::ExistingFile);

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
    // Only the "On" column (0) carries the enabled checkbox
    if (!item || item->column() != 0) return;
    const int row = item->row();
    if (row < 0 || row >= static_cast<int>(m_definitions.size())) return;

    const bool on = (item->checkState() == Qt::Checked);
    m_definitions[static_cast<size_t>(row)].enabled = on;

    // Update row text colour to reflect enabled state
    {
        const QSignalBlocker blocker(m_table);
        for (int c = 1; c < 4; ++c)
            if (auto* it = m_table->item(row, c))
                it->setForeground(on ? QColor() : QColor(150, 150, 150));
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
    dlg.setMinimumWidth(420);

    auto* form = new QFormLayout();

    auto* nameEdit    = new QLineEdit(QString::fromStdString(def.name),    &dlg);
    auto* fieldEdit   = new QLineEdit(QString::fromStdString(def.field),   &dlg);
    auto* patternEdit = new QLineEdit(QString::fromStdString(def.pattern), &dlg);
    auto* enabledBox  = new QCheckBox(tr("Enabled"), &dlg);
    enabledBox->setChecked(def.enabled);

    fieldEdit->setPlaceholderText(tr("(empty = any field)"));
    patternEdit->setPlaceholderText(tr("Regular expression …"));

    auto* statusLabel = new QLabel(&dlg);
    statusLabel->setWordWrap(true);

    form->addRow(tr("Name:"),    nameEdit);
    form->addRow(tr("Field:"),   fieldEdit);
    form->addRow(tr("Pattern:"), patternEdit);
    form->addRow(QString(),      enabledBox);
    form->addRow(QString(),      statusLabel);

    // Live regexp validation
    auto validate = [&]() {
        const QString pat = patternEdit->text().trimmed();
        if (pat.isEmpty())
        {
            statusLabel->setText(QString());
            statusLabel->setStyleSheet(QString());
            return;
        }
        QRegularExpression re(pat);
        if (re.isValid())
        {
            statusLabel->setText(tr("✓ Valid regexp"));
            statusLabel->setStyleSheet("color: green;");
        }
        else
        {
            statusLabel->setText(tr("✗ ") + re.errorString());
            statusLabel->setStyleSheet("color: red;");
        }
    };
    connect(patternEdit, &QLineEdit::textChanged, &dlg, validate);
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
        dlg.accept();
    });
    connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);

    auto* mainLayout = new QVBoxLayout(&dlg);
    mainLayout->addLayout(form);
    mainLayout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) return false;

    def.name    = nameEdit->text().trimmed().toStdString();
    def.field   = fieldEdit->text().trimmed().toStdString();
    def.pattern = patternEdit->text().trimmed().toStdString();
    def.enabled = enabledBox->isChecked();
    return true;
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void ActorDefinitionsPanel::EmitAndSave()
{
    emit DefinitionsChanged(m_definitions);
    // Auto-save to the current file (silently)
    if (!m_currentFilePath.empty())
    {
        try
        {
            std::ofstream ofs(m_currentFilePath);
            if (ofs)
                ofs << ActorDefinition::ListToJson(m_definitions).dump(2);
        }
        catch (...) {}
    }
}

std::string ActorDefinitionsPanel::DefaultFilePath()
{
    return (config::GetConfig().GetDefaultAppPath() / "actors.json").string();
}

} // namespace ui::qt
