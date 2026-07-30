#include "ShortcutsDialog.hpp"

#include <QVBoxLayout>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QPushButton>
#include <QKeySequence>

namespace ui::qt
{

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

    // Create table
    m_table = new QTableWidget(this);
    m_table->setColumnCount(3);
    m_table->setHorizontalHeaderLabels({"Category", "Action", "Shortcut"});
    m_table->horizontalHeader()->setStretchLastSection(false);
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);

    layout->addWidget(m_table);

    // Close button
    auto* closeBtn = new QPushButton("Close", this);
    connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
    layout->addWidget(closeBtn);
}

void ShortcutsDialog::populateShortcuts()
{
    // File menu
    addShortcut("Open", "Ctrl+O", "File");
    addShortcut("Save Session", "Ctrl+Shift+S", "File");
    addShortcut("Open Session", "Ctrl+Shift+O", "File");
    addShortcut("Clear Data", "Ctrl+Shift+L", "File");
    addShortcut("Follow File (Tail)", "Ctrl+T", "File");
    addShortcut("Exit", "Ctrl+Q", "File");

    // Edit menu
    addShortcut("Copy", "Ctrl+C", "Edit");
    addShortcut("Find", "Ctrl+F", "Edit");

    // View menu
    addShortcut("Show Bookmarks", "Ctrl+B", "View");
    addShortcut("Side by Side Comparison", "Ctrl+Shift+C", "View");

    // Tools menu
    addShortcut("Reload Plugins", "Ctrl+Shift+P", "Tools");
    addShortcut("Preferences", "Cmd+,", "Tools");

    // Analysis
    addShortcut("Jump to Timestamp", "Ctrl+Shift+T", "Analysis");
    addShortcut("Next Search Result", "F3 or Ctrl+G", "Search");
    addShortcut("Previous Search Result", "Shift+F3 or Ctrl+Shift+G", "Search");

    // Resizing
    addShortcut("Fit Column Width", "Double-click column header", "Columns");
    addShortcut("Drag Column", "Click and drag column header", "Columns");
}

void ShortcutsDialog::addShortcut(const QString& action, const QString& shortcut, const QString& category)
{
    int row = m_table->rowCount();
    m_table->insertRow(row);

    auto* categoryItem = new QTableWidgetItem(category);
    auto* actionItem = new QTableWidgetItem(action);
    auto* shortcutItem = new QTableWidgetItem(shortcut);

    // Style shortcut column
    shortcutItem->setFont(QFont("Monospace", 10));

    m_table->setItem(row, 0, categoryItem);
    m_table->setItem(row, 1, actionItem);
    m_table->setItem(row, 2, shortcutItem);
}

}  // namespace ui::qt
