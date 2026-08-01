#include "MainWindowExportOpsHelper.hpp"
#include "MainWindow.hpp"
#include "EventsContainer.hpp"
#include "Logger.hpp"

#include <QFileDialog>
#include <QStandardPaths>

namespace ui::qt {

MainWindowExportOpsHelper::MainWindowExportOpsHelper(MainWindow* mainWindow)
    : m_mainWindow(mainWindow),
      m_eventsContainer(nullptr),
      m_lastExportPath(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
{
    if (!m_mainWindow) {
        util::Logger::Error("[ExportOpsHelper] MainWindow pointer is null");
    }
}

void MainWindowExportOpsHelper::OnExportCsvRequested()
{
    if (!m_mainWindow || !m_eventsContainer) return;

    util::Logger::Debug("[ExportOpsHelper] CSV export requested");
    PerformExport("csv");
}

void MainWindowExportOpsHelper::OnExportJsonRequested()
{
    if (!m_mainWindow || !m_eventsContainer) return;

    util::Logger::Debug("[ExportOpsHelper] JSON export requested");
    PerformExport("json");
}

void MainWindowExportOpsHelper::OnExportXmlRequested()
{
    if (!m_mainWindow || !m_eventsContainer) return;

    util::Logger::Debug("[ExportOpsHelper] XML export requested");
    PerformExport("xml");
}

void MainWindowExportOpsHelper::OnGenerateReportFromDashboard()
{
    if (!m_mainWindow || !m_eventsContainer) return;

    util::Logger::Debug("[ExportOpsHelper] Generate report from dashboard requested");

    // Delegate to MainWindow for report generation
}

void MainWindowExportOpsHelper::PerformExport(const QString& format)
{
    if (!m_mainWindow || !m_eventsContainer) return;

    QString fileName = QFileDialog::getSaveFileName(
        m_mainWindow,
        QObject::tr("Export As %1").arg(format.toUpper()),
        m_lastExportPath,
        QObject::tr("%1 Files (*.%2);;All Files (*)").arg(format.toUpper()).arg(format)
    );

    if (fileName.isEmpty()) return;

    if (!ValidateExportPath(fileName)) {
        util::Logger::Warn("[ExportOpsHelper] Invalid export path: {}", fileName.toStdString());
        return;
    }

    m_lastExportPath = fileName;

    util::Logger::Info("[ExportOpsHelper] Exporting to {} format: {}", format.toStdString(), fileName.toStdString());

    // Delegate actual export to MainWindow or ExportManager
}

bool MainWindowExportOpsHelper::ValidateExportPath(const QString& path) const
{
    if (path.isEmpty()) return false;

    // Basic validation - more sophisticated checks delegated to ExportManager
    return true;
}

} // namespace ui::qt
