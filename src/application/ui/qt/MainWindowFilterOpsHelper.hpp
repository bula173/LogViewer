#pragma once

#include <QString>
#include <memory>

class QMainWindow;

namespace filters { class FilterManager; }
namespace db { class EventsContainer; }
namespace ui::qt {

class MainWindow;

/**
 * @brief Helper class encapsulating filter and search operations for MainWindow
 *
 * Delegates filter/search operations from MainWindow, reducing its complexity.
 * Manages:
 * - Filter application and coordination
 * - Search orchestration
 * - Filter status updates
 * - Extended filter handling
 *
 * Maintains reference to FilterManager for state management.
 */
class MainWindowFilterOpsHelper {
public:
    explicit MainWindowFilterOpsHelper(MainWindow* mainWindow);
    ~MainWindowFilterOpsHelper() = default;

    // Filter operations
    void OnApplyFilterClicked();
    void OnExtendedFiltersChanged();
    void OnSearchRequested();
    void OnSearchResultActivated(long eventId);
    void ApplyExtendedFilters();
    void UpdateFilterStatus(int totalEvents, int filteredCount, const QString& filterName);

    // State accessors
    bool HasActiveFilters() const;
    int GetCurrentFilteredCount() const { return m_lastFilteredCount; }

private:
    MainWindow* m_mainWindow;
    filters::FilterManager* m_filterManager;
    db::EventsContainer* m_eventsContainer;
    int m_lastFilteredCount = 0;
    QString m_lastFilterName;

    void ApplyFilterInternal();
    void RefreshFilterIndicators();
};

} // namespace ui::qt
