#include "EventsTableModel.hpp"

#include <QBrush>
#include <QModelIndex>
#include <QString>
#include <QVariant>

#include "LogEvent.hpp"

#include <algorithm>
#include <limits>
#include <numeric>

namespace ui::qt
{

EventsTableModel::EventsTableModel(db::EventsContainer& events,
    QObject* parent)
    : QAbstractTableModel(parent)
    , m_events(events)
    , m_config(config::GetConfig())
{
    RebuildVisibleColumns();
}

int EventsTableModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    // If filtering is active, use filtered indices (even if empty)
    // If filtering is not active, show all events
    const std::size_t sourceSize = m_filteringActive ? m_filteredIndices.size() : m_events.Size();
    const auto maxInt = static_cast<std::size_t>(std::numeric_limits<int>::max());
    return static_cast<int>(std::min(sourceSize, maxInt));
}

int EventsTableModel::columnCount(const QModelIndex& parent) const
{
    if (parent.isValid())
        return 0;

    return static_cast<int>(m_visibleColumnIndices.size());
}

QVariant EventsTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return {};

    const int actualIndex = ResolveToActualIndex(index.row());
    if (actualIndex < 0)
        return {};

    // Safety check: ensure actualIndex is within bounds
    if (actualIndex >= static_cast<int>(m_events.Size()))
        return {};

    const auto& columns = m_config.GetColumns();
    if (index.column() < 0 ||
        index.column() >= static_cast<int>(m_visibleColumnIndices.size()))
        return {};

    const int columnConfigIndex = m_visibleColumnIndices[static_cast<std::size_t>(index.column())];
    
    // Handle dynamic source column (index -1) and original_id column (index -2)
    std::string columnName;
    if (columnConfigIndex == -1)
    {
        columnName = "source";
    }
    else if (columnConfigIndex == -2)
    {
        columnName = "original_id";
    }
    else if (columnConfigIndex < 0 || columnConfigIndex >= static_cast<int>(columns.size()))
    {
        return {};
    }
    else
    {
        columnName = columns[static_cast<std::size_t>(columnConfigIndex)].name;
    }
    
    const auto& event = m_events.GetEvent(actualIndex);

    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::EditRole:
            return ComposeCellText(event, columnName);
        case Qt::ForegroundRole:
        case Qt::BackgroundRole:
        {
            // Apply color to entire row based on configured typeFilterField
            const std::string typeValue = event.findByKey(m_config.typeFilterField);
            const QColor color = ResolveColor(m_config.typeFilterField, typeValue, role == Qt::BackgroundRole);
            if (color.isValid())
                return QBrush(color);
            break;
        }
        default:
            break;
    }

    return {};
}

QVariant EventsTableModel::headerData(int section,
    Qt::Orientation orientation, int role) const
{
    if (role != Qt::DisplayRole)
        return {};

    if (orientation == Qt::Horizontal)
    {
        if (section < 0 ||
            section >= static_cast<int>(m_visibleColumnIndices.size()))
            return {};

        const int columnIndex = m_visibleColumnIndices[static_cast<std::size_t>(section)];
        
        // Handle dynamic source column
        if (columnIndex == -1)
        {
            return QString("Source");
        }
        
        // Handle dynamic original_id column
        if (columnIndex == -2)
        {
            return QString("Original ID");
        }
        
        const auto& columns = m_config.GetColumns();
        if (columnIndex < 0 || columnIndex >=
                static_cast<int>(columns.size()))
            return {};

        return QString::fromStdString(
            columns[static_cast<std::size_t>(columnIndex)].name);
    }

    return section + 1;
}

void EventsTableModel::SyncWithContainer()
{
    RefreshAll();
}

void EventsTableModel::RefreshAll()
{
    beginResetModel();
    endResetModel();
}

void EventsTableModel::RefreshColumns()
{
    // Store old column count
    const int oldColumnCount = static_cast<int>(m_visibleColumnIndices.size());
    
    // Rebuild the visible columns list based on current config
    RebuildVisibleColumns();
    
    const int newColumnCount = static_cast<int>(m_visibleColumnIndices.size());
    
    // If column count changed, notify the view properly
    if (newColumnCount != oldColumnCount)
    {
        // Model structure changed - need to reset
        beginResetModel();
        endResetModel();
    }
    else
    {
        // Just refresh the data
        RefreshAll();
    }
}

void EventsTableModel::UpdateColors()
{
    const int rows = rowCount(QModelIndex());
    const int columns = columnCount(QModelIndex());
    if (rows == 0 || columns == 0)
        return;

    const QModelIndex topLeft = index(0, 0);
    const QModelIndex bottomRight = index(rows - 1, columns - 1);
    emit dataChanged(topLeft, bottomRight,
        {Qt::ForegroundRole, Qt::BackgroundRole});
}

void EventsTableModel::SetFilteredIndices(
    const std::vector<unsigned long>& indices)
{
    m_filteredIndices = indices;
    m_filteringActive = true; // Mark that filtering is now active
    
    // Build reverse lookup map for O(1) RowFromActualIndex lookups
    m_reverseFilteredIndices.clear();
    m_reverseFilteredIndices.reserve(indices.size());
    for (int row = 0; row < static_cast<int>(indices.size()); ++row)
    {
        m_reverseFilteredIndices[indices[static_cast<std::size_t>(row)]] = row;
    }
    
    RefreshAll();
}

void EventsTableModel::ClearFilter()
{
    m_filteredIndices.clear();
    m_reverseFilteredIndices.clear();
    m_filteringActive = false; // Mark that filtering is no longer active
    RefreshAll();
}

int EventsTableModel::ResolveToActualIndex(int row) const
{
    if (row < 0)
        return -1;

    if (m_filteringActive)
    {
        if (row >= static_cast<int>(m_filteredIndices.size()))
            return -1;
        return static_cast<int>(m_filteredIndices[static_cast<std::size_t>(row)]);
    }

    return row;
}

int EventsTableModel::RowFromActualIndex(int actualIndex) const
{
    if (actualIndex < 0)
        return -1;

    if (!m_filteringActive)
        return actualIndex;

    // Use reverse lookup map for O(1) performance instead of O(n) linear search
    const auto it = m_reverseFilteredIndices.find(static_cast<unsigned long>(actualIndex));
    if (it == m_reverseFilteredIndices.end())
        return -1;

    return it->second;
}

std::vector<int> EventsTableModel::ColumnWidths() const
{
    std::vector<int> widths;
    widths.reserve(m_visibleColumnIndices.size());
    const auto& columns = m_config.GetColumns();
    for (const int idx : m_visibleColumnIndices)
    {
        if (idx == -1)
        {
            // Source column default width
            widths.push_back(100);
        }
        else if (idx == -2)
        {
            // Original ID column default width
            widths.push_back(100);
        }
        else if (idx >= 0 && idx < static_cast<int>(columns.size()))
        {
            widths.push_back(columns[static_cast<std::size_t>(idx)].width);
        }
    }
    return widths;
}

void EventsTableModel::RebuildVisibleColumns()
{
    m_visibleColumnIndices.clear();
    const auto& columns = m_config.GetColumns();
    
    // Check if we should show source column
    const bool needsSourceColumn = ShouldShowSourceColumn();
    const bool needsOriginalIdColumn = ShouldShowOriginalIdColumn();
    
    // Build list of visible columns
    for (std::size_t i = 0; i < columns.size(); ++i)
    {
        if (columns[i].isVisible)
        {
            m_visibleColumnIndices.push_back(static_cast<int>(i));
            
            // Insert dynamic columns after id column (if id is first visible column)
            if (i == 0 && columns[i].name == "id")
            {
                if (needsSourceColumn)
                    m_visibleColumnIndices.push_back(-1); // -1 indicates source column
                if (needsOriginalIdColumn)
                    m_visibleColumnIndices.push_back(-2); // -2 indicates original_id column
            }
        }
    }
    
    // If dynamic columns should be shown but weren't added after id, add them at the beginning
    if (needsSourceColumn)
    {
        bool sourceAdded = false;
        for (int idx : m_visibleColumnIndices)
        {
            if (idx == -1)
            {
                sourceAdded = true;
                break;
            }
        }
        
        if (!sourceAdded)
        {
            // Insert source at the beginning
            m_visibleColumnIndices.insert(m_visibleColumnIndices.begin(), -1);
        }
    }

    if (needsOriginalIdColumn)
    {
        bool originalIdAdded = false;
        for (int idx : m_visibleColumnIndices)
        {
            if (idx == -2)
            {
                originalIdAdded = true;
                break;
            }
        }
        
        if (!originalIdAdded)
        {
            // Insert after source column if present, otherwise at beginning
            auto sourcePos = std::find(m_visibleColumnIndices.begin(), 
                                      m_visibleColumnIndices.end(), -1);
            if (sourcePos != m_visibleColumnIndices.end())
            {
                m_visibleColumnIndices.insert(sourcePos + 1, -2);
            }
            else
            {
                m_visibleColumnIndices.insert(m_visibleColumnIndices.begin(), -2);
            }
        }
    }
    
    m_hasSourceColumn = needsSourceColumn;
}

bool EventsTableModel::ShouldShowSourceColumn() const
{
    // Check if any event has a source set
    const size_t count = m_events.Size();
    // Check first few events for performance (or check all if small dataset)
    const size_t samplesToCheck = std::min(count, size_t(100));
    
    for (size_t i = 0; i < samplesToCheck; ++i)
    {
        const auto& event = m_events.GetEvent(static_cast<int>(i));
        if (!event.GetSource().empty())
        {
            return true;
        }
    }
    
    return false;
}

bool EventsTableModel::ShouldShowOriginalIdColumn() const
{
    // Check if any event has an original_id field (set during merge)
    const size_t count = m_events.Size();
    // Check first few events for performance
    const size_t samplesToCheck = std::min(count, size_t(100));
    
    for (size_t i = 0; i < samplesToCheck; ++i)
    {
        const auto& event = m_events.GetEvent(static_cast<int>(i));
        const std::string originalId = event.findByKey("original_id");
        if (!originalId.empty())
        {
            return true;
        }
    }
    
    return false;
}

QString EventsTableModel::ComposeCellText(const db::LogEvent& event,
    const std::string& columnName) const
{
    if (columnName == "id")
        return QString::number(event.getId());
    
    if (columnName == "source")
        return QString::fromStdString(event.GetSource());
    
    if (columnName == "original_id")
    {
        const std::string originalId = event.findByKey("original_id");
        return QString::fromStdString(originalId);
    }

    const auto values = event.findAllByKey(columnName);
    if (values.empty())
        return {};

    if (values.size() == 1)
        return QString::fromStdString(values[0]);
        

    // Pre-calculate size for all values plus separators
    size_t totalSize = 0;
    for (const auto& value : values)
        totalSize += value.length();
    totalSize += (values.size() - 1) * 2; // ", " separators

    std::string combined;
    combined.reserve(totalSize);

    for (size_t i = 0; i < values.size(); ++i)
    {
        if (i > 0)
            combined += ", ";
        combined += values[i];
    }

    return QString::fromStdString(combined);
}

QColor EventsTableModel::ResolveColor(const std::string& column,
    const std::string& value, bool background) const
{
    if (column.empty() || value.empty())
        return {};

    const auto& colorMap = m_config.columnColors;
    const auto columnIt = colorMap.find(column);
    if (columnIt == colorMap.end())
        return {};

    const auto valueIt = columnIt->second.find(value);
    if (valueIt == columnIt->second.end())
        return {};

    const auto& colorCode = background ? valueIt->second.bg : valueIt->second.fg;
    QColor color(QString::fromStdString(colorCode));
    return color.isValid() ? color : QColor {};
}

QVariant EventsTableModel::GetSortValue(const db::LogEvent& event,
    const std::string& columnName) const
{
    if (columnName == "id")
        return QVariant::fromValue(static_cast<long long>(event.getId()));
    
    if (columnName == "source")
        return QString::fromStdString(event.GetSource());
    
    if (columnName == "original_id")
    {
        const std::string originalId = event.findByKey("original_id");
        if (originalId.empty())
            return QString();
        // Try to parse as number for numeric comparison
        bool ok = false;
        long long numValue = QString::fromStdString(originalId).toLongLong(&ok);
        if (ok)
            return QVariant::fromValue(numValue);
        return QString::fromStdString(originalId);
    }

    const auto values = event.findAllByKey(columnName);
    if (values.empty())
        return QString();

    const std::string& value = values[0];
    
    // Try to parse as number (for timestamps and numeric fields)
    bool ok = false;
    long long numValue = QString::fromStdString(value).toLongLong(&ok);
    if (ok)
        return QVariant::fromValue(numValue);
    
    // Try as double
    double doubleValue = QString::fromStdString(value).toDouble(&ok);
    if (ok)
        return QVariant::fromValue(doubleValue);
    
    // Fall back to string comparison
    return QString::fromStdString(value);
}

void EventsTableModel::sort(int column, Qt::SortOrder order)
{
    if (column < 0 || column >= static_cast<int>(m_visibleColumnIndices.size()))
        return;

    const int columnConfigIndex = m_visibleColumnIndices[static_cast<std::size_t>(column)];
    
    // Determine column name
    std::string columnName;
    if (columnConfigIndex == -1)
    {
        columnName = "source";
    }
    else if (columnConfigIndex == -2)
    {
        columnName = "original_id";
    }
    else
    {
        const auto& columns = m_config.GetColumns();
        if (columnConfigIndex < 0 || columnConfigIndex >= static_cast<int>(columns.size()))
            return;
        columnName = columns[static_cast<std::size_t>(columnConfigIndex)].name;
    }

    emit layoutAboutToBeChanged();

    // Get the list of indices to sort
    std::vector<unsigned long> indicesToSort;
    if (m_filteredIndices.empty())
    {
        // Sort all events
        indicesToSort.resize(m_events.Size());
        std::iota(indicesToSort.begin(), indicesToSort.end(), 0UL);
    }
    else
    {
        // Sort only filtered events
        indicesToSort = m_filteredIndices;
    }

    // Sort the indices based on the column values
    std::sort(indicesToSort.begin(), indicesToSort.end(),
        [this, &columnName, order](unsigned long a, unsigned long b) {
            const auto& eventA = m_events.GetEvent(static_cast<int>(a));
            const auto& eventB = m_events.GetEvent(static_cast<int>(b));
            
            QVariant valA = GetSortValue(eventA, columnName);
            QVariant valB = GetSortValue(eventB, columnName);
            
            // Handle different types
            if (valA.typeId() == QMetaType::LongLong && valB.typeId() == QMetaType::LongLong)
            {
                long long aNum = valA.toLongLong();
                long long bNum = valB.toLongLong();
                return order == Qt::AscendingOrder ? aNum < bNum : aNum > bNum;
            }
            else if (valA.typeId() == QMetaType::Double && valB.typeId() == QMetaType::Double)
            {
                double aNum = valA.toDouble();
                double bNum = valB.toDouble();
                return order == Qt::AscendingOrder ? aNum < bNum : aNum > bNum;
            }
            else
            {
                // String comparison
                QString aStr = valA.toString();
                QString bStr = valB.toString();
                int cmp = aStr.compare(bStr, Qt::CaseInsensitive);
                return order == Qt::AscendingOrder ? cmp < 0 : cmp > 0;
            }
        });

    // Update the filtered indices with sorted order
    if (m_filteredIndices.empty())
    {
        // We sorted all events, but we don't set m_filteredIndices
        // because empty m_filteredIndices means "show all"
        // Instead, we need to actually reorder the container
        // However, we can't reorder the container itself as it may break other references
        // So we set the filtered indices to the sorted order
        m_filteredIndices = indicesToSort;
        // When we populated filtered indices from a full sort, mark filtering
        // as active so other model methods use the mapped indices.
        m_filteringActive = true;
        // Rebuild reverse lookup map for RowFromActualIndex
        m_reverseFilteredIndices.clear();
        m_reverseFilteredIndices.reserve(m_filteredIndices.size());
        for (int row = 0; row < static_cast<int>(m_filteredIndices.size()); ++row)
        {
            m_reverseFilteredIndices[m_filteredIndices[static_cast<std::size_t>(row)]] = row;
        }
    }
    else
    {
        m_filteredIndices = indicesToSort;
    }

    emit layoutChanged();
}

} // namespace ui::qt
