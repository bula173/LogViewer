#pragma once

#include <QString>
#include <memory>

class QMainWindow;

namespace db { class EventsContainer; }
namespace ui::qt {

class MainWindow;

/**
 * @brief Helper class encapsulating export and reporting operations for MainWindow
 *
 * Delegates export/reporting operations from MainWindow, reducing its complexity.
 * Manages:
 * - CSV export
 * - JSON export
 * - XML export
 * - Report generation
 *
 * Coordinates with ExportManager for actual export operations.
 */
class MainWindowExportOpsHelper {
public:
    explicit MainWindowExportOpsHelper(MainWindow* mainWindow);
    ~MainWindowExportOpsHelper() = default;

    // Export operations
    void OnExportCsvRequested();
    void OnExportJsonRequested();
    void OnExportXmlRequested();
    void OnGenerateReportFromDashboard();

    // State accessors
    QString GetLastExportPath() const { return m_lastExportPath; }
    void SetLastExportPath(const QString& path) { m_lastExportPath = path; }

private:
    MainWindow* m_mainWindow;
    db::EventsContainer* m_eventsContainer;
    QString m_lastExportPath;

    void PerformExport(const QString& format);
    bool ValidateExportPath(const QString& path) const;
};

} // namespace ui::qt
