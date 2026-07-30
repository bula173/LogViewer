#pragma once

#include <QWidget>
#include <QString>
#include <vector>
#include <string>

class QLabel;
class QPushButton;

namespace ui::qt
{

/**
 * @brief Status bar widget showing active filters and match counts.
 *
 * Displays:
 * - Number of active filters
 * - Total events vs. filtered results
 * - List of active filter types
 * - Clear all button
 *
 * Example: "📊 2 filters active | 1,234 / 50,000 events | Type: ERROR,WARN"
 */
class FilterStatusBar : public QWidget
{
    Q_OBJECT

  public:
    explicit FilterStatusBar(QWidget* parent = nullptr);

    /**
     * @brief Update the status bar with filter information.
     *
     * @param totalEvents Total number of events in container
     * @param filteredCount Number of events matching current filters
     * @param activeFilterCount Number of active filters
     * @param filterDetails Description of active filters (e.g., "Type: ERROR,WARN")
     */
    void UpdateFilterStatus(int totalEvents, int filteredCount,
                           int activeFilterCount,
                           const QString& filterDetails);

    /**
     * @brief Show that all events are visible (no filtering).
     */
    void ClearStatus();

  Q_SIGNALS:
    void ClearAllFiltersRequested();

  private:
    void CreateLayout();

    QLabel* m_statusLabel {nullptr};
    QPushButton* m_clearButton {nullptr};
};

} // namespace ui::qt
