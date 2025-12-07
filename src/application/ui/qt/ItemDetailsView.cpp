#include "ui/qt/ItemDetailsView.hpp"

#include "db/EventsContainer.hpp"
#include "db/LogEvent.hpp"

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

namespace {

/**
 * @brief Convert hex string to ASCII representation
 * @param hexStr Hex string (e.g., "48656c6c6f")
 * @return ASCII string or original if not valid hex
 */
QString HexToAscii(const std::string& hexStr)
{
    // Check if string looks like hex (even length, only hex chars)
    if (hexStr.empty() || hexStr.length() % 2 != 0)
        return QString::fromStdString(hexStr);
    
    // Check if all characters are hex digits
    bool isHex = std::all_of(hexStr.begin(), hexStr.end(), 
        [](char c) { return std::isxdigit(static_cast<unsigned char>(c)); });
    
    if (!isHex)
        return QString::fromStdString(hexStr);
    
    // Convert hex to ASCII
    std::string ascii;
    ascii.reserve(hexStr.length() / 2);
    
    for (size_t i = 0; i < hexStr.length(); i += 2)
    {
        std::string byteStr = hexStr.substr(i, 2);
        char byte = static_cast<char>(std::stoi(byteStr, nullptr, 16));
        
        // Only include printable ASCII characters
        if (byte >= 32 && byte <= 126)
            ascii += byte;
        else
            ascii += '.'; // Replace non-printable with dot
    }
    
    return QString::fromStdString(ascii);
}

/**
 * @brief Check if key name indicates hex-encoded text
 */
bool IsHexTextField(const std::string& key)
{
    return key.find("_text") != std::string::npos ||
           key.find("Text") != std::string::npos ||
           key.find("x_text") != std::string::npos;
}

} // anonymous namespace

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

    int row = 0;
    for (const auto& [key, value] : items)
    {
        auto* keyItem =
            new QTableWidgetItem(QString::fromStdString(key));
        
        // Convert hex to ASCII if key name indicates hex-encoded text
        QString displayValue;
        if (IsHexTextField(key))
        {
            displayValue = HexToAscii(value);
            // Show both hex and ASCII in tooltip
            QString tooltip = QString("Hex: %1\nASCII: %2")
                .arg(QString::fromStdString(value))
                .arg(displayValue);
            
            auto* valueItem = new QTableWidgetItem(displayValue);
            valueItem->setToolTip(tooltip);
            valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
            valueItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
            
            keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
            keyItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
            
            m_details->setItem(row, 0, keyItem);
            m_details->setItem(row, 1, valueItem);
        }
        else
        {
            auto* valueItem =
                new QTableWidgetItem(QString::fromStdString(value));
            
            keyItem->setFlags(keyItem->flags() & ~Qt::ItemIsEditable);
            valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);
            keyItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
            valueItem->setTextAlignment(Qt::AlignLeft | Qt::AlignTop);
            
            m_details->setItem(row, 0, keyItem);
            m_details->setItem(row, 1, valueItem);
        }
        
        ++row;
    }
    m_details->resizeColumnsToContents();
    m_details->resizeRowsToContents();
}

} // namespace ui::qt
