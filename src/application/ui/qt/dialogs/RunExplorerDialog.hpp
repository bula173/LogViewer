#pragma once

#include "analyzers/TestRunIndex.hpp"

#include <QDialog>
#include <QFutureWatcher>
#include <QString>

#include <vector>

class QAction;
class QCheckBox;
class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace ui::qt
{

/**
 * @brief Run Explorer: the tests of one results folder (one safeAPI merged log
 *        per test), with verdict, duration, entry count and failure message.
 *
 * A non-modal dialog: it stays open next to the main window while logs are
 * opened from it. The folder is scanned on a worker thread
 * (analyzer::ScanResultsFolder through QtConcurrent) with a progress bar and
 * a Cancel button. Rows can be sorted by any column, filtered by verdict, and
 * failures grouped by their normalised message.
 *
 * Activating a test row emits OpenTestRequested(); "Open with Previous Test"
 * emits OpenWithPreviousRequested() with the test that ran just before it.
 */
class RunExplorerDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit RunExplorerDialog(QWidget* parent = nullptr);
    ~RunExplorerDialog() override;

    /// Scans @p folder in the background (a running scan is cancelled first).
    void OpenFolder(const QString& folder);

    /// Shows @p entries, which must be in run order (as ScanResultsFolder returns them).
    void SetEntries(const QString& folder, std::vector<analyzer::TestRunEntry> entries);

    [[nodiscard]] const std::vector<analyzer::TestRunEntry>& Entries() const { return m_entries; }

  signals:
    void OpenTestRequested(const QString& path);
    void OpenWithPreviousRequested(const QString& previousPath, const QString& previousName,
                                   const QString& path, const QString& name);
    /// A scan started by OpenFolder() ended (finished or cancelled).
    void ScanFinished();

  private:
    void OnScanFinished();
    void Rebuild();
    void ApplyVerdictFilter();
    void UpdateActions();
    [[nodiscard]] int CurrentEntry() const; ///< entry index of the current row, -1 for none / a group
    [[nodiscard]] QTreeWidgetItem* CreateTestItem(int index) const;
    void OpenCurrent();
    void OpenCurrentWithPrevious();

    QLabel*       m_summary {nullptr};
    QComboBox*    m_verdictFilter {nullptr};
    QCheckBox*    m_groupByMessage {nullptr};
    QProgressBar* m_progress {nullptr};
    QPushButton*  m_cancelButton {nullptr};
    QTreeWidget*  m_tree {nullptr};
    QPushButton*  m_openButton {nullptr};
    QPushButton*  m_openWithPreviousButton {nullptr};
    QAction*      m_openAction {nullptr};
    QAction*      m_openWithPreviousAction {nullptr};

    QFutureWatcher<std::vector<analyzer::TestRunEntry>>* m_watcher {nullptr};
    QString                                m_folder;
    std::vector<analyzer::TestRunEntry>    m_entries;
};

} // namespace ui::qt
