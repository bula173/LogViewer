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

    // If we're already displaying this event, no need to update
    if (actualRow == m_currentlyDisplayedRow)
        return;

    if (actualRow < 0 || actualRow >= static_cast<int>(m_events.Size()))
    {
        m_details->setRowCount(0);
        m_currentlyDisplayedRow = -1;
        return;
    }

    const auto& event = m_events.GetItem(actualRow);
    const auto& items = event.getEventItems();
    const int itemCount = static_cast<int>(items.size());
    
    // Only update row count if it changed
    if (m_details->rowCount() != itemCount)
    {
        m_details->setRowCount(itemCount);
    }

    // Get field dictionary from config
    const auto& dictionary = config::GetConfig().GetFieldTranslator();

    int row = 0;
    for (const auto& [key, value] : items)
    {
        // Reuse existing items if possible
        QTableWidgetItem* keyItem = m_details->item(row, 0);
        QTableWidgetItem* valueItem = m_details->item(row, 1);
        
        const QString keyStr = QString::fromStdString(key);
        if (!keyItem)
        {
            keyItem = new QTableWidgetItem(keyStr);
            keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
            keyItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
            m_details->setItem(row, 0, keyItem);
        }
        else if (keyItem->text() != keyStr)
        {
            keyItem->setText(keyStr);
        }

        QFont font = keyItem->font();
        font.setBold(true);
        keyItem->setFont(font);
        
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
        
        if (!valueItem)
        {
            valueItem = new QTableWidgetItem(displayValue);
            valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
            valueItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
            m_details->setItem(row, 1, valueItem);
        }
        else if (valueItem->text() != displayValue)
        {
            valueItem->setText(displayValue);
        }
        
        // Check for item-specific highlighting configuration
        const auto& itemHighlights = config::GetConfig().itemHighlights;
        const auto highlightIt = itemHighlights.find(key);
        
        // Always clear previous highlighting first
        keyItem->setBackground(QBrush());
        keyItem->setForeground(QBrush());
        valueItem->setBackground(QBrush());
        valueItem->setForeground(QBrush());
        
        // Reset font styles
        QFont valueFont = valueItem->font();
        valueFont.setBold(false);
        valueFont.setItalic(false);
        valueItem->setFont(valueFont);
        
        // Apply highlighting if configured
        if (highlightIt != itemHighlights.end())
        {
            const auto& highlight = highlightIt->second;
            
            // Apply font styling
            if (highlight.bold)
                valueFont.setBold(true);
            if (highlight.italic)
                valueFont.setItalic(true);
            valueItem->setFont(valueFont);
            
            // Apply background color
            if (!highlight.backgroundColor.empty())
            {
                QColor bgColor(QString::fromStdString(highlight.backgroundColor));
                if (bgColor.isValid())
                {
                    keyItem->setBackground(QBrush(bgColor));
                    valueItem->setBackground(QBrush(bgColor));
                }
            }
            
            // Apply foreground color
            if (!highlight.foregroundColor.empty())
            {
                QColor fgColor(QString::fromStdString(highlight.foregroundColor));
                if (fgColor.isValid())
                {
                    keyItem->setForeground(QBrush(fgColor));
                    valueItem->setForeground(QBrush(fgColor));
                }
            }
        }
        
        if (!tooltip.isEmpty())
        {
            valueItem->setToolTip(tooltip);
        }
        else if (!valueItem->toolTip().isEmpty())
        {
            valueItem->setToolTip(QString());
        }
        
        ++row;
    }
    m_details->resizeColumnsToContents();
    m_details->resizeRowsToContents();
    
    m_currentlyDisplayedRow = actualRow;
}

} // namespace ui::qt
