#include "ItemDetailsView.hpp"

#include "EventsContainer.hpp"
#include "LogEvent.hpp"
#include "Config.hpp"

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
#include <vector>

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

namespace
{

constexpr const char* kSignalPrefix = "SIG:";
constexpr int kSignalPrefixLen = 4; // strlen("SIG:")

bool IsSignalKey(const std::string& key)
{
    return key.size() > static_cast<size_t>(kSignalPrefixLen) &&
           key.substr(0, kSignalPrefixLen) == kSignalPrefix;
}

// Append a full-width separator row spanning both columns.
// Assumes the row already exists (appended via setRowCount or insertRow by caller).
void SetSeparatorRow(QTableWidget* table, int row, const QString& label)
{
    auto* item = new QTableWidgetItem(label);
    item->setFlags(Qt::ItemIsEnabled);
    item->setTextAlignment(Qt::AlignCenter);
    QFont f = item->font();
    f.setBold(true);
    f.setPointSizeF(f.pointSizeF() * 0.85);
    item->setFont(f);
    const QColor sep = table->palette().color(QPalette::AlternateBase).darker(110);
    item->setBackground(QBrush(sep));
    table->setItem(row, 0, item);
    // Column 1 placeholder so the span covers it visually.
    auto* blank = new QTableWidgetItem();
    blank->setFlags(Qt::ItemIsEnabled);
    blank->setBackground(QBrush(sep));
    table->setItem(row, 1, blank);
    table->setSpan(row, 0, 1, 2);
}

} // namespace

void ItemDetailsView::DisplayEvent(int actualRow)
{
    if (!m_details)
        return;

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

    // Partition into regular fields and SIG: signal fields.
    std::vector<std::pair<std::string, std::string>> regularItems;
    std::vector<std::pair<std::string, std::string>> signalItems;
    for (const auto& [key, value] : items)
    {
        if (key == "original_id")
            continue;
        if (IsSignalKey(key))
            signalItems.emplace_back(key, value);
        else
            regularItems.emplace_back(key, value);
    }

    // Total rows = regular + separator (if signals present) + signals.
    const bool hasSignals = !signalItems.empty();
    const int totalRows = static_cast<int>(regularItems.size())
                        + (hasSignals ? 1 : 0)
                        + static_cast<int>(signalItems.size());

    // Reset and repopulate to keep logic simple (events rarely change schema).
    m_details->clearSpans();
    m_details->setRowCount(0);
    m_details->setRowCount(totalRows);

    const auto& dictionary   = config::GetConfig().GetFieldTranslator();
    const auto& itemHighlights = config::GetConfig().itemHighlights;

    // Palette-based signal background — adapts to light/dark theme.
    const QColor signalBg = m_details->palette()
                                .color(QPalette::AlternateBase)
                                .lighter(108);

    auto populateRow = [&](int row,
                           const std::string& key,
                           const std::string& value,
                           bool isSignal)
    {
        // Key column: strip "SIG:" prefix for readability.
        const QString displayKey = isSignal
            ? QString::fromStdString(key.substr(kSignalPrefixLen))
            : QString::fromStdString(key);

        auto* keyItem = new QTableWidgetItem(displayKey);
        keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
        keyItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
        QFont keyFont = keyItem->font();
        keyFont.setBold(true);
        keyItem->setFont(keyFont);
        m_details->setItem(row, 0, keyItem);

        // Value column: apply field-translator if configured.
        QString displayValue = QString::fromStdString(value);
        QString tooltip;
        if (dictionary.HasTranslation(key))
        {
            auto result = dictionary.Translate(key, value);
            if (result.wasConverted)
                displayValue = QString::fromStdString(result.convertedValue);
            if (!result.tooltip.empty())
                tooltip = QString::fromStdString(result.tooltip);
        }

        auto* valueItem = new QTableWidgetItem(displayValue);
        valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
        valueItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
        if (!tooltip.isEmpty())
            valueItem->setToolTip(tooltip);
        m_details->setItem(row, 1, valueItem);

        // Default: apply theme-aware signal background.
        if (isSignal)
        {
            keyItem->setBackground(QBrush(signalBg));
            valueItem->setBackground(QBrush(signalBg));
        }

        // Config-defined per-key highlights override defaults.
        const auto highlightIt = itemHighlights.find(key);
        if (highlightIt != itemHighlights.end())
        {
            const auto& h = highlightIt->second;
            QFont vf = valueItem->font();
            if (h.bold)   vf.setBold(true);
            if (h.italic) vf.setItalic(true);
            valueItem->setFont(vf);
            if (!h.backgroundColor.empty())
            {
                QColor bg(QString::fromStdString(h.backgroundColor));
                if (bg.isValid())
                {
                    keyItem->setBackground(QBrush(bg));
                    valueItem->setBackground(QBrush(bg));
                }
            }
            if (!h.foregroundColor.empty())
            {
                QColor fg(QString::fromStdString(h.foregroundColor));
                if (fg.isValid())
                {
                    keyItem->setForeground(QBrush(fg));
                    valueItem->setForeground(QBrush(fg));
                }
            }
        }
    };

    int row = 0;
    for (const auto& [key, value] : regularItems)
        populateRow(row++, key, value, false);

    if (hasSignals)
    {
        SetSeparatorRow(m_details, row++, tr("— Decoded CAN Signals —"));
        for (const auto& [key, value] : signalItems)
            populateRow(row++, key, value, true);
    }

    m_details->resizeColumnsToContents();
    m_details->resizeRowsToContents();
    m_currentlyDisplayedRow = actualRow;
}

} // namespace ui::qt
