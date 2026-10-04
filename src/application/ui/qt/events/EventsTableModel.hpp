#pragma once

#include <QAbstractTableModel>
#include <QColor>
#include <QSet>
#include <QString>

#include "Config.hpp"
#include "EventsContainer.hpp"
#include "utils/SearchEngine.hpp"

#include <cstdint>
#include <map>
#include <set>
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
    bool narrowed {false};                ///< Other filters hid rows, so values may be missing
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
    /// The rows chosen by the rest of the application (SetFilteredIndices),
    /// before column filters and sorting; nullptr when no such filter is set.
    const std::vector<unsigned long>* GetBaseFilteredIndices() const
    {
        return m_baseFilterActive ? &m_baseFilteredIndices : nullptr;
    }
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
    /// True if a column filter of a *visible* column restricts the rows
    /// (filters of hidden columns are kept but not applied).
    bool HasActiveColumnFilter() const;
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
    /// Index into Config::GetColumns() shown by model @p column; -1 for the
    /// dynamic source / original_id columns or an invalid column.
    int ConfigIndexForColumn(int column) const;
    /// Model column of the active sort, or -1 if there is none or its column is hidden.
    int ActiveSortColumn() const;
    Qt::SortOrder ActiveSortOrder() const { return m_sortOrder; }

  signals:
    void ColumnFiltersChanged();
    /// Search matches were recomputed after the row set changed.
    void SearchMatchesChanged();

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
    /// Sort key of one row, computed once (see SortIndices / SortBefore).
    struct SortKey
    {
        unsigned long index {0};
        int           id {0};
        bool          numeric {false};
        bool          isInt {false};
        long long     asInt {0};
        double        asDouble {0.0};
        QString       text;
    };
    SortKey MakeSortKey(unsigned long idx, const std::string& columnName, bool isMergeSource) const;
    /// Three-way ascending comparison of two keys (ties broken by event id).
    static int CompareSortKeys(const SortKey& a, const SortKey& b);
    /// True if row @p a sorts before row @p b in the active sort.
    bool SortBefore(unsigned long a, unsigned long b) const;
    /// Slots rows appended since the last full sort into the sorted list
    /// (O(k log k + n) instead of a full re-sort). Falls back to a full
    /// re-filter when the sorted list does not cover the previous events.
    void AppendSortedRows(std::size_t total);
    /// Drops column filters and the sort whose column is gone from the column
    /// configuration (renamed / removed) and reports what was dropped.
    struct PrunedColumnState
    {
        bool filters {false};
        bool sort {false};
    };
    PrunedColumnState PruneStaleColumnState();
    /// True if the upstream filter belongs to an older container generation
    /// or points past the container's end.
    bool BaseFilterIsStale() const;
    /// True if the upstream filter or the effective row list is stale.
    bool HasStaleIndices() const;
    /// Ends a model reset begun with beginResetModel(): repairs stale row
    /// state, then re-applies search highlighting. Row-set changes must be made
    /// after beginResetModel() so views can still map their selection to events.
    void EndReset();
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
    std::uint64_t m_baseGeneration {0};    ///< container generation the upstream filter was set for
    std::uint64_t m_indicesGeneration {0}; ///< container generation m_filteredIndices was built for
    std::map<std::string, ColumnFilter> m_columnFilters;
    std::set<std::string> m_visibleFilterKeys; ///< Keys of the visible columns; only their filters apply
    bool          m_hasSort {false};
    /// Events covered by m_filteredIndices when it is the complete sorted list
    /// (no upstream or column filter); 0 when it is not such a list.
    std::size_t   m_fullSortedCount {0};
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
