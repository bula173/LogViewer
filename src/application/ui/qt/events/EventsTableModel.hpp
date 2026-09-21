#pragma once

#include <QAbstractTableModel>
#include <QColor>
#include <QSet>
#include <QString>

#include "Config.hpp"
#include "EventsContainer.hpp"
#include "utils/SearchEngine.hpp"

#include <map>
#include <unordered_set>
#include <vector>
#include <unordered_map>
#include <memory>

namespace ui::qt
{

/// One distinct cell value of a column and how many rows show it.
struct ColumnValueCount
{
    QString    value;
    qsizetype  count {0};
};

/// Distinct values offered by an Excel-style column filter.
struct ColumnDistinctValues
{
    std::vector<ColumnValueCount> values; ///< Sorted (numeric-aware, case-insensitive)
    bool truncated {false};               ///< More distinct values exist than were listed
};

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
    /// True if a filter is active — distinguishes "no filter" from "filter active, zero matches".
    bool IsFilteringActive() const { return m_filteringActive; }

    // ── Search / highlight ────────────────────────────────────────────────
    void SetSearchTerm(const QString& term, bool caseSensitive);
    void SetSearchMode(utils::SearchMode mode);
    int  MatchCount() const { return static_cast<int>(m_matchedRows.size()); }
    const std::vector<int>& MatchedRows() const { return m_matchedRows; }
    const utils::SearchEngine& GetSearchEngine() const { return *m_searchEngine; }

    /// Asynchronous search rebuild (doesn't block UI)
    void RebuildSearchMatchesAsync();

    // ── Column value filters (Excel-style) ────────────────────────────────
    // Each column may restrict the rows to a set of allowed cell values; all
    // column filters are ANDed with each other and with whatever filter the
    // rest of the application set through SetFilteredIndices()/ClearFilter().
    // Filters are keyed by column name, so they survive column reordering.

    /// Distinct cell texts of @p column among the rows that pass every filter
    /// *except* this column's own (so the list narrows as other columns are
    /// filtered, like Excel). At most @p maxValues values are returned.
    ColumnDistinctValues DistinctColumnValues(int column, std::size_t maxValues) const;
    bool HasColumnFilter(int column) const;
    bool HasAnyColumnFilter() const { return !m_columnFilters.empty(); }
    /// Values of @p column's filter (empty set if none): the allowed values, or
    /// the excluded ones when IsColumnFilterExclusion().
    QSet<QString> ColumnFilterValues(int column) const;
    /// True if @p column's filter hides the listed values instead of showing them.
    bool IsColumnFilterExclusion(int column) const;
    /// Restricts @p column to @p allowed (an empty set matches nothing).
    void SetColumnFilter(int column, const QSet<QString>& allowed);
    /// Hides the @p excluded values of @p column and keeps every other value
    /// (used when the value list was too long to show completely).
    void SetColumnFilterExcluding(int column, const QSet<QString>& excluded);
    void ClearColumnFilter(int column);
    void ClearColumnFilters();

    int ResolveToActualIndex(int row) const;
    int RowFromActualIndex(int actualIndex) const;
    std::vector<int> ColumnWidths() const;

  signals:
    void ColumnFiltersChanged();

  private:
    struct ColumnFilter
    {
        std::string   name;
        bool          mergeSource {false};
        QSet<QString> allowed;      ///< allowed values, or excluded ones when @c exclude
        bool          exclude {false};
    };

    /// Resolves a model column to its data-column name / merge-source flag.
    bool ResolveColumn(int column, std::string& name, bool& mergeSource) const;
    static std::string ColumnFilterKey(const std::string& name, bool mergeSource);
    bool PassesColumnFilters(const db::LogEvent& event, const std::string* skipKey) const;
    /// Rebuilds m_filteredIndices from the upstream filter + column filters
    /// (+ the active sort). Does not reset the model — callers do.
    void ApplyEffectiveFilter();
    void SortIndices(std::vector<unsigned long>& indices, const std::string& columnName,
        bool isMergeSource, Qt::SortOrder order) const;
    /// Drops column filters and the sort whose column is no longer visible
    /// (renamed / hidden in the column configuration). Returns true if a
    /// column filter was dropped.
    bool PruneStaleColumnState();
    void StoreColumnFilter(int column, const QSet<QString>& values, bool exclude);

    void RebuildVisibleColumns();
    bool ShouldShowSourceColumn() const;
    bool ShouldShowOriginalIdColumn() const;
    /// @p mergeSource: the column is the dynamic multi-file "source" column
    /// (shows LogEvent::GetSource()). A configured data column that merely
    /// happens to be named "source" reads the event's own "source" field.
    QString ComposeCellText(const db::LogEvent& event,
        const std::string& columnName, bool mergeSource) const;
    QVariant GetSortValue(const db::LogEvent& event,
        const std::string& columnName, bool mergeSource) const;

    db::EventsContainer& m_events;
    std::vector<unsigned long> m_filteredIndices;  ///< Effective rows (upstream ∩ column filters, sorted if m_hasSort)
    std::vector<unsigned long> m_baseFilteredIndices; ///< Rows chosen by the rest of the app
    bool m_baseFilterActive {false};
    std::map<std::string, ColumnFilter> m_columnFilters;
    bool          m_hasSort {false};
    std::string   m_sortName;          ///< sort column by data name, so it survives column changes
    bool          m_sortMergeSource {false};
    Qt::SortOrder m_sortOrder {Qt::AscendingOrder};
    std::unordered_map<unsigned long, int> m_reverseFilteredIndices; // actual index -> filtered row
    std::vector<int> m_visibleColumnIndices;
    const config::Config& m_config;
    bool m_hasSourceColumn {false}; // Track if source column is currently shown
    bool m_filteringActive {false}; // Track if filtering is active (distinguish empty from no filter)

    // Search
    std::unique_ptr<utils::SearchEngine> m_searchEngine;
    QString                    m_searchTerm;
    bool                       m_searchCaseSensitive {false};
    std::vector<int>           m_matchedRows;    ///< model row indices with at least one matching cell
    std::unordered_set<int>    m_matchedRowSet;  ///< O(1) lookup used in data()

    void RebuildSearchMatches();
};

} // namespace ui::qt
