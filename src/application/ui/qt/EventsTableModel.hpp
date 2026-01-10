#pragma once

#include <QAbstractTableModel>
#include <QColor>

#include "Config.hpp"
#include "EventsContainer.hpp"

#include <vector>

#include <unordered_map>

namespace ui::qt
{

class EventsTableModel : public QAbstractTableModel
{
    Q_OBJECT

  public:
    explicit EventsTableModel(db::EventsContainer& events,
        QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation,
        int role) const override;
    void sort(int column, Qt::SortOrder order = Qt::AscendingOrder) override;

    void SyncWithContainer();
    void RefreshAll();
    void RefreshColumns();
    void UpdateColors();
    void SetFilteredIndices(const std::vector<unsigned long>& indices);
    void ClearFilter(); // Clear filtering and show all events
    const std::vector<unsigned long>& GetFilteredIndices() const { return m_filteredIndices; }

    int ResolveToModelIndex(int row) const;
    int RowFromModelIndex(int actualIndex) const;
    std::vector<int> ColumnWidths() const;

  private:
    void RebuildVisibleColumns();
    bool ShouldShowSourceColumn() const;
    bool ShouldShowOriginalIdColumn() const;
    QString ComposeCellText(const db::LogEvent& event,
        const std::string& columnName) const;
    QColor ResolveColor(const std::string& column,
        const std::string& value, bool background) const;
    QVariant GetSortValue(const db::LogEvent& event,
        const std::string& columnName) const;

    db::EventsContainer& m_events;
    std::vector<unsigned long> m_filteredIndices;
    std::unordered_map<unsigned long, int> m_reverseFilteredIndices; // actual index -> filtered row
    std::vector<int> m_visibleColumnIndices;
    const config::Config& m_config;
    bool m_hasSourceColumn {false}; // Track if source column is currently shown
    bool m_filteringActive {false}; // Track if filtering is active (distinguish empty from no filter)
};

} // namespace ui::qt
