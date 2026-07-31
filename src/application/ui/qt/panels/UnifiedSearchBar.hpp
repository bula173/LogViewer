#pragma once

#include <QWidget>
#include <QString>
#include <QStringList>
#include <QSettings>
#include <memory>

class QLineEdit;
class QPushButton;
class QLabel;
class QCompleter;

namespace db {
class EventsContainer;
}

namespace ui::qt {
class EventsTableView;

/**
 * @brief Unified search bar for global event search and filtering.
 *
 * Provides:
 * - Type-ahead search across all event fields
 * - Live match counter
 * - Recent searches history
 * - Filter presets for quick access
 * - Keyboard shortcuts (Ctrl+F)
 *
 * The search bar integrates with EventsTableView to navigate and filter
 * matching events, and can save searches for quick re-use.
 */
class UnifiedSearchBar : public QWidget
{
    Q_OBJECT

  public:
    explicit UnifiedSearchBar(QWidget* parent = nullptr);
    ~UnifiedSearchBar() = default;

    /// Set the events source for searching
    void SetEventsSource(db::EventsContainer* events);

    /// Set the events view for navigation
    void SetEventsView(EventsTableView* view);

    /// Focus the search input (called by Ctrl+F shortcut)
    void FocusSearchInput();

    /// Get the current search query
    QString GetSearchQuery() const;

    /// Clear search and results
    void ClearSearch();

  signals:
    /// Emitted when search query changes
    void SearchChanged(const QString& query);

    /// Emitted when user wants to apply as filter
    void FilterRequested(const QString& field, const QString& value);

    /// Emitted when search matches change
    void MatchCountChanged(int current, int total);

  private slots:
    void OnSearchTextChanged(const QString& text);
    void OnSearchReturn();
    void OnClearClicked();
    void OnSearchHistoryItemClicked(const QString& text);
    void OnQuickFilterClicked();
    void OnSearchSuggestionClicked(const QString& suggestion);

  private:
    void CreateLayout();
    void UpdateMatchCount();
    void LoadSearchHistory();
    void SaveSearchHistory();
    void AddToSearchHistory(const QString& query);
    void CreateFilterPresets();

    /// Count events matching the search query
    int CountMatches(const QString& query);

    db::EventsContainer* m_events = nullptr;
    EventsTableView* m_eventsView = nullptr;

    // UI components
    QLineEdit* m_searchInput = nullptr;
    QLabel* m_matchCountLabel = nullptr;
    QPushButton* m_clearButton = nullptr;
    QPushButton* m_filterButton = nullptr;
    QCompleter* m_completer = nullptr;

    // Settings
    std::unique_ptr<QSettings> m_settings;
    QStringList m_searchHistory;
    static constexpr int MAX_HISTORY = 10;

    // State
    int m_lastMatchCount = 0;
    QString m_lastQuery;
    QTimer* m_matchCountDebounceTimer = nullptr;
    static constexpr int MATCH_COUNT_DEBOUNCE_MS = 300;
};

} // namespace ui::qt
