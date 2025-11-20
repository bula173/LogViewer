#include "ui/qt/EventsTableModel.hpp"

#include <QBrush>
#include <QModelIndex>
#include <QString>
#include <QVariant>

#include "db/LogEvent.hpp"

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

    const std::size_t sourceSize = m_filteredIndices.empty() ? m_events.Size() :
        m_filteredIndices.size();
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

    const auto& columns = m_config.GetColumns();
    if (index.column() < 0 ||
        index.column() >= static_cast<int>(m_visibleColumnIndices.size()))
        return {};

    const int columnConfigIndex = m_visibleColumnIndices[index.column()];
    if (columnConfigIndex < 0 || columnConfigIndex >= static_cast<int>(columns.size()))
        return {};

    const auto& columnName = columns[static_cast<std::size_t>(columnConfigIndex)].name;
    const auto& event = m_events.GetEvent(actualIndex);

    switch (role)
    {
        case Qt::DisplayRole:
        case Qt::EditRole:
            return ComposeCellText(event, columnName);
        case Qt::ForegroundRole:
        {
            const QColor color = ResolveColor(columnName,
                event.findByKey(columnName), false);
            if (color.isValid())
                return QBrush(color);
            break;
        }
        case Qt::BackgroundRole:
        {
            const QColor color = ResolveColor(columnName,
                event.findByKey(columnName), true);
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

        const auto& columns = m_config.GetColumns();
        const int columnIndex = m_visibleColumnIndices[section];
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
    RebuildVisibleColumns();
    RefreshAll();
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
    RefreshAll();
}

int EventsTableModel::ResolveToActualIndex(int row) const
{
    if (row < 0)
        return -1;

    if (!m_filteredIndices.empty())
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

    if (m_filteredIndices.empty())
        return actualIndex;

    const auto it = std::find(
        m_filteredIndices.begin(), m_filteredIndices.end(),
        static_cast<unsigned long>(actualIndex));
    if (it == m_filteredIndices.end())
        return -1;

    return static_cast<int>(std::distance(m_filteredIndices.begin(), it));
}

std::vector<int> EventsTableModel::ColumnWidths() const
{
    std::vector<int> widths;
    widths.reserve(m_visibleColumnIndices.size());
    const auto& columns = m_config.GetColumns();
    for (const int idx : m_visibleColumnIndices)
    {
        if (idx >= 0 && idx < static_cast<int>(columns.size()))
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
    for (std::size_t i = 0; i < columns.size(); ++i)
    {
        if (columns[i].isVisible)
            m_visibleColumnIndices.push_back(static_cast<int>(i));
    }
}

QString EventsTableModel::ComposeCellText(const db::LogEvent& event,
    const std::string& columnName) const
{
    if (columnName == "id")
        return QString::number(event.getId());

    const auto values = event.findAllByKey(columnName);
    if (values.empty())
        return {};

    std::string combined;
    for (const auto& value : values)
    {
        if (!combined.empty())
            combined += ", ";
        combined += value;
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

} // namespace ui::qt
