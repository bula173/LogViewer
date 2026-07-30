#include "PanelNavigator.hpp"
#include <QVBoxLayout>
#include <QListWidget>
#include <QLineEdit>
#include <QKeyEvent>

namespace ui::qt::widgets
{

PanelNavigator::PanelNavigator(QWidget* parent)
    : QDialog(parent)
{
    setWindowTitle("Panel Navigator");
    setModal(true);
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground, false);

    // Layout
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Search input
    m_searchInput = new QLineEdit(this);
    m_searchInput->setPlaceholderText("Quick panel search... (type to filter)");
    m_searchInput->setStyleSheet(
        "QLineEdit { "
        "  padding: 8px; "
        "  border: none; "
        "  border-bottom: 1px solid #ccc; "
        "  font-size: 13px; "
        "}"
    );
    layout->addWidget(m_searchInput);

    // Panel list
    m_panelList = new QListWidget(this);
    m_panelList->setStyleSheet(
        "QListWidget { "
        "  border: none; "
        "  outline: none; "
        "} "
        "QListWidget::item { "
        "  padding: 6px 12px; "
        "  border-bottom: 1px solid #eee; "
        "} "
        "QListWidget::item:hover { "
        "  background-color: #f0f0f0; "
        "} "
        "QListWidget::item:selected { "
        "  background-color: #007bff; "
        "  color: white; "
        "}"
    );
    layout->addWidget(m_panelList);

    // Connect signals
    connect(m_searchInput, &QLineEdit::textChanged,
            this, &PanelNavigator::onSearchTextChanged);
    connect(m_panelList, &QListWidget::itemDoubleClicked,
            this, &PanelNavigator::onItemDoubleClicked);
    connect(m_panelList, &QListWidget::itemEntered,
            this, [this](QListWidgetItem* item) {
        m_panelList->setCurrentItem(item);
    });

    setMinimumWidth(400);
    setMaximumHeight(500);
}

void PanelNavigator::addPanel(const PanelInfo& panel)
{
    m_allPanels.push_back(panel);
}

void PanelNavigator::clearPanels()
{
    m_allPanels.clear();
    m_panelList->clear();
}

void PanelNavigator::show()
{
    updateFilteredList();
    m_searchInput->clear();
    m_searchInput->setFocus();

    // Center on parent
    if (parentWidget()) {
        QRect parentGeometry = parentWidget()->geometry();
        int x = parentGeometry.x() + (parentGeometry.width() - width()) / 2;
        int y = parentGeometry.y() + (parentGeometry.height() - height()) / 3;
        move(x, y);
    }

    QDialog::show();
}

void PanelNavigator::onSearchTextChanged(const QString&)
{
    updateFilteredList();
}

void PanelNavigator::updateFilteredList()
{
    m_panelList->clear();

    QString searchText = m_searchInput->text().toLower();

    for (const auto& panel : m_allPanels) {
        if (searchText.isEmpty() ||
            panel.name.toLower().contains(searchText) ||
            panel.category.toLower().contains(searchText)) {

            QString displayText = QString("%1 %2 (%3)")
                .arg(panel.icon)
                .arg(panel.name)
                .arg(panel.category);

            auto* item = new QListWidgetItem(displayText, m_panelList);
            item->setData(Qt::UserRole, QVariant::fromValue(
                reinterpret_cast<quintptr>(&panel)));
        }
    }

    if (m_panelList->count() > 0) {
        m_panelList->setCurrentRow(0);
    }
}

void PanelNavigator::onItemDoubleClicked(QListWidgetItem*)
{
    selectCurrentItem();
}

void PanelNavigator::onItemHighlighted(int)
{
    // Handle highlight if needed
}

void PanelNavigator::onEnterPressed()
{
    selectCurrentItem();
}

void PanelNavigator::selectCurrentItem()
{
    auto* currentItem = m_panelList->currentItem();
    if (!currentItem) return;

    // Find matching panel
    QString itemText = currentItem->text();
    for (const auto& panel : m_allPanels) {
        if (itemText.contains(panel.name)) {
            panel.onSelect();
            close();
            return;
        }
    }
}

}  // namespace ui::qt::widgets
