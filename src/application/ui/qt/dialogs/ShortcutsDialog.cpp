#include "ShortcutsDialog.hpp"

#include "utils/ShortcutManager.hpp"

#include <QDialogButtonBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequenceEdit>
#include <QLabel>
#include <QMessageBox>
#include <QPrintDialog>
#include <QPrinter>
#include <QPushButton>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QTextDocument>
#include <QVBoxLayout>

#include <optional>

namespace ui::qt {

namespace {

/// Modal capture dialog: lets the user press a new key combination.
std::optional<QKeySequence> CaptureShortcut(QWidget* parent, const QString& forLabel)
{
    QDialog dlg(parent);
    dlg.setWindowTitle(QObject::tr("Edit Shortcut"));
    dlg.setModal(true);

    auto* layout = new QVBoxLayout(&dlg);
    layout->addWidget(new QLabel(
        QObject::tr("Press the new key combination for \"%1\":").arg(forLabel), &dlg));

    auto* edit = new QKeySequenceEdit(&dlg);
    layout->addWidget(edit);

    auto* buttons = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dlg);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    layout->addWidget(buttons);

    if (dlg.exec() != QDialog::Accepted) return std::nullopt;

    // QKeySequenceEdit can capture up to 4 chained key sequences; only the
    // first is relevant for a single-shortcut rebind.
    const auto sequences = edit->keySequence();
    return sequences;
}

} // namespace

ShortcutsDialog::ShortcutsDialog(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Keyboard Shortcuts");
    setModal(true);
    setMinimumWidth(600);
    setMinimumHeight(500);

    setupUI();
    populateShortcuts();
}

ShortcutsDialog::~ShortcutsDialog() = default;

void ShortcutsDialog::setupUI()
{
    auto* layout = new QVBoxLayout(this);

    auto* hint = new QLabel(
        tr("Double-click a shortcut to change it."), this);
    hint->setStyleSheet("font-style: italic;");
    layout->addWidget(hint);

    m_table = new QTableWidget(this);
    m_table->setObjectName("shortcutsTable");
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({"Category", "Action", "Shortcut"});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    connect(m_table, &QTableWidget::cellDoubleClicked,
        this, &ShortcutsDialog::HandleCellDoubleClicked);

    layout->addWidget(m_table);

    auto* btnRow = new QHBoxLayout();
    m_editBtn = new QPushButton(tr("Edit Shortcut…"), this);
    m_editBtn->setObjectName("shortcutsEditButton");
    connect(m_editBtn, &QPushButton::clicked, this, &ShortcutsDialog::HandleEditSelected);
    btnRow->addWidget(m_editBtn);

    m_resetBtn = new QPushButton(tr("Reset to Default"), this);
    m_resetBtn->setObjectName("shortcutsResetButton");
    connect(m_resetBtn, &QPushButton::clicked, this, &ShortcutsDialog::HandleResetSelected);
    btnRow->addWidget(m_resetBtn);

    auto* resetAllBtn = new QPushButton(tr("Reset All"), this);
    resetAllBtn->setObjectName("shortcutsResetAllButton");
    connect(resetAllBtn, &QPushButton::clicked, this, &ShortcutsDialog::HandleResetAll);
    btnRow->addWidget(resetAllBtn);

    auto* printBtn = new QPushButton(tr("Print…"), this);
    printBtn->setObjectName("shortcutsPrintButton");
    connect(printBtn, &QPushButton::clicked, this, &ShortcutsDialog::HandlePrint);
    btnRow->addWidget(printBtn);

    btnRow->addStretch(1);

    auto* closeBtn = new QPushButton(tr("Close"), this);
    closeBtn->setObjectName("shortcutsCloseButton");
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    btnRow->addWidget(closeBtn);

    layout->addLayout(btnRow);
}

void ShortcutsDialog::populateShortcuts()
{
    m_table->setRowCount(0);
    m_rowIds.clear();

    auto addRow = [this](const QString& category, const QString& action,
                          const QString& shortcut, const std::string& id)
    {
        const int row = m_table->rowCount();
        m_table->insertRow(row);

        auto* categoryItem = new QTableWidgetItem(category);
        auto* actionItem   = new QTableWidgetItem(action);
        auto* shortcutItem = new QTableWidgetItem(shortcut);
        shortcutItem->setFont(QFont("Monospace", 10));

        m_table->setItem(row, 0, categoryItem);
        m_table->setItem(row, 1, actionItem);
        m_table->setItem(row, 2, shortcutItem);
        m_rowIds.push_back(id);
    };

    for (const auto& s : ShortcutManager::getInstance().GetAll())
    {
        addRow(s.category, s.label, s.sequence.toString(QKeySequence::NativeText), s.id);
    }

    // Reference-only entries: standard-key shortcuts owned by individual
    // widgets (not app-wide QActions) and mouse gestures. Not customizable.
    addRow(tr("Edit"), tr("Copy"), tr("Ctrl+C"), "");
    addRow(tr("Edit"), tr("Find"), tr("Ctrl+F"), "");
    addRow(tr("Columns"), tr("Fit Column Width"), tr("Double-click column header"), "");
    addRow(tr("Columns"), tr("Drag Column"), tr("Click and drag column header"), "");
}

void ShortcutsDialog::HandleCellDoubleClicked(int row, int /*column*/)
{
    EditRow(row);
}

void ShortcutsDialog::HandleEditSelected()
{
    const int row = m_table->currentRow();
    if (row < 0) return;
    EditRow(row);
}

void ShortcutsDialog::EditRow(int row)
{
    if (row < 0 || row >= static_cast<int>(m_rowIds.size())) return;
    const std::string& id = m_rowIds[static_cast<size_t>(row)];
    if (id.empty()) return; // reference-only entry, not customizable

    const QString label = m_table->item(row, 1)->text();
    const auto captured = CaptureShortcut(this, label);
    if (!captured) return;

    const auto result = ShortcutManager::getInstance().SetShortcut(id, *captured);
    if (!result.isOk())
    {
        QMessageBox::warning(this, tr("Cannot Assign Shortcut"),
            tr("\"%1\" is %2.").arg(captured->toString(QKeySequence::NativeText),
                QString::fromStdString(result.error().what())));
        return;
    }
    populateShortcuts();
}

void ShortcutsDialog::HandleResetSelected()
{
    const int row = m_table->currentRow();
    if (row < 0 || row >= static_cast<int>(m_rowIds.size())) return;
    const std::string& id = m_rowIds[static_cast<size_t>(row)];
    if (id.empty()) return;

    ShortcutManager::getInstance().ResetToDefault(id);
    populateShortcuts();
}

void ShortcutsDialog::HandleResetAll()
{
    if (QMessageBox::question(this, tr("Reset All Shortcuts"),
            tr("Restore every shortcut to its default binding?")) != QMessageBox::Yes)
        return;

    ShortcutManager::getInstance().ResetAllToDefaults();
    populateShortcuts();
}

void ShortcutsDialog::HandlePrint()
{
    QString html = "<h2>LogViewer Keyboard Shortcuts</h2><table border='1' cellspacing='0' cellpadding='4'>";
    html += "<tr><th>Category</th><th>Action</th><th>Shortcut</th></tr>";
    for (int row = 0; row < m_table->rowCount(); ++row)
    {
        html += "<tr><td>" + m_table->item(row, 0)->text().toHtmlEscaped()
              + "</td><td>" + m_table->item(row, 1)->text().toHtmlEscaped()
              + "</td><td>" + m_table->item(row, 2)->text().toHtmlEscaped()
              + "</td></tr>";
    }
    html += "</table>";

    QTextDocument doc;
    doc.setHtml(html);

    QPrinter printer(QPrinter::HighResolution);
    QPrintDialog printDialog(&printer, this);
    if (printDialog.exec() != QDialog::Accepted) return;
    doc.print(&printer);
}

}  // namespace ui::qt
