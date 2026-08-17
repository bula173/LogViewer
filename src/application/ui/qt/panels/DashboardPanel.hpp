#pragma once

#include <QWidget>
#include <QString>
#include <memory>

class QLabel;
class QPushButton;
class QScrollArea;

namespace db {
class EventsContainer;
}

namespace ui::qt {

/**
 * @brief Dashboard panel showing log file overview and quick statistics.
 *
 * Displays:
 * - File info (name, format, size, time range)
 * - Event statistics (total count, breakdown by the configured type filter
 *   field — config::GetConfig().typeFilterField, e.g. "level" — shown as
 *   whatever distinct values actually occur in that column, not a fixed
 *   ERROR/WARN/INFO/DEBUG vocabulary)
 * - Top actors/services
 * - Quick action buttons (Export, Report, Bookmark)
 *
 * Updates automatically when events change.
 */
class DashboardPanel : public QWidget
{
    Q_OBJECT

  public:
    explicit DashboardPanel(QWidget* parent = nullptr);
    ~DashboardPanel() = default;

    /// Set the events container to display stats for
    void SetEventsSource(db::EventsContainer* events);

    /// Manually trigger stats update (called automatically on events change)
    void UpdateStats();

    /// Refresh dashboard (called from lazy refresh cycle)
    void Refresh() { UpdateStats(); }

    /// Force immediate recalculation of statistics
    void RecalculateStats() { UpdateStats(); }

  signals:
    void ExportRequested();
    void GenerateReportRequested();
    void BookmarkCurrentRequested();

  private slots:
    void OnExportClicked();
    void OnReportClicked();
    void OnBookmarkClicked();
    void OnEventsChanged();

  private:
    void CreateLayout();
    void UpdateFileInfo();
    void UpdateEventStats();
    void UpdateTopActors();

    /// Helper to format large numbers (1234567 → "1.2M")
    static QString FormatNumber(qint64 count);

    /// Helper to get log level icon/color
    static QString GetLevelIcon(const QString& level);

    db::EventsContainer* m_events = nullptr;

    // UI components
    QLabel* m_fileNameLabel = nullptr;
    QLabel* m_fileFormatLabel = nullptr;
    QLabel* m_fileSizeLabel = nullptr;
    QLabel* m_timeRangeLabel = nullptr;

    QLabel* m_totalEventsLabel = nullptr;
    QLabel* m_typeBreakdownTitleLabel = nullptr;
    QLabel* m_typeBreakdownLabel = nullptr;

    QLabel* m_topActorsLabel = nullptr;

    QPushButton* m_exportButton = nullptr;
    QPushButton* m_reportButton = nullptr;
    QPushButton* m_bookmarkButton = nullptr;

    QScrollArea* m_scrollArea = nullptr;
};

} // namespace ui::qt
