#include "ui/qt/ItemDetailsView.hpp"

#include "db/EventsContainer.hpp"
#include "db/LogEvent.hpp"
#include "config/Config.hpp"

#include <QAction>
#include <QClipboard>
#include <QHeaderView>
#include <QGuiApplication>
#include <QKeySequence>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QTableWidgetItem>

#include <algorithm>
#include <sstream>
#include <iomanip>

namespace ui::qt
{

ItemDetailsView::ItemDetailsView(db::EventsContainer& events, QWidget* parent)
    : QWidget(parent)
    , m_events(events)
    , m_details(new QTableWidget(this))
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_details);

    m_details->setColumnCount(2);
    QStringList headers;
    headers << "Key" << "Value";
    m_details->setHorizontalHeaderLabels(headers);
    m_details->horizontalHeader()->setStretchLastSection(true);
    m_details->verticalHeader()->setVisible(false);
    m_details->verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_details->setSelectionBehavior(QAbstractItemView::SelectItems);
    m_details->setSelectionMode(QAbstractItemView::SingleSelection);
    m_details->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_details->setWordWrap(true);
    m_details->setContextMenuPolicy(Qt::ActionsContextMenu);

    auto* copyAction = new QAction(tr("Copy"), m_details);
    copyAction->setShortcut(QKeySequence::Copy);
    m_details->addAction(copyAction);

    connect(copyAction, &QAction::triggered, this, [this]() {
        const auto items = m_details->selectedItems();
        if (items.isEmpty())
            return;

        QStringList lines;
        for (auto* item : items)
            lines << item->text();

        QClipboard* clipboard = QGuiApplication::clipboard();
        clipboard->setText(lines.join('\n'));
    });

    m_events.RegisterOndDataUpdated(this);
}

void ItemDetailsView::RefreshView()
{
    const int currentIndex = m_events.GetCurrentItemIndex();
    // Only display if we have data and a valid index
    if (m_events.Size() > 0 && currentIndex >= 0)
    {
        DisplayEvent(currentIndex);
    }
    else
    {
        // Clear the display if no data or invalid index
        if (m_details)
            m_details->setRowCount(0);
    }
}

void ItemDetailsView::ShowControl(bool show)
{
    setVisible(show);
}

void ItemDetailsView::OnDataUpdated()
{
    RefreshView();
}

void ItemDetailsView::OnCurrentIndexUpdated(const int index)
{
    DisplayEvent(index);
}

void ItemDetailsView::OnActualRowChanged(int actualRow)
{
    DisplayEvent(actualRow);
}

void ItemDetailsView::DisplayEvent(int actualRow)
{
    if (!m_details)
        return;

    if (actualRow < 0 || actualRow >= static_cast<int>(m_events.Size()))
    {
        m_details->setRowCount(0);
        return;
    }

    const auto& event = m_events.GetItem(actualRow);
    const auto& items = event.getEventItems();
    m_details->setRowCount(static_cast<int>(items.size()));

    // Get field dictionary from config
    const auto& dictionary = config::GetConfig().GetFieldTranslator();

    int row = 0;
    for (const auto& [key, value] : items)
    {
        auto* keyItem = new QTableWidgetItem(QString::fromStdString(key));
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        keyItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
        
        // Try to look up value in dictionary if configured
        QString displayValue = QString::fromStdString(value);
        QString tooltip;
        
        if (dictionary.HasTranslation(key))
        {
            auto result = dictionary.Translate(key, value);
            if (result.wasConverted)
            {
                displayValue = QString::fromStdString(result.convertedValue);
            }
            // Set tooltip regardless of whether value was converted
            if (!result.tooltip.empty())
            {
                tooltip = QString::fromStdString(result.tooltip);
            }
        }
        
        auto* valueItem = new QTableWidgetItem(displayValue);
        valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        valueItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
        
        if (!tooltip.isEmpty())
        {
            valueItem->setToolTip(tooltip);
        }
        
        m_details->setItem(row, 0, keyItem);
        m_details->setItem(row, 1, valueItem);
        
        ++row;
    }
    m_details->resizeColumnsToContents();
    m_details->resizeRowsToContents();
}

} // namespace ui::qt
