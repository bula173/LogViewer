#pragma once

#include <QDialog>
#include <QString>
#include <vector>

class QComboBox;
class QPushButton;
class QCheckBox;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class QGroupBox;

namespace db {
class EventsContainer;
}

namespace ui::qt {
class EventsTableView;
}

namespace ui::qt {

/**
 * @brief Enhanced export dialog for v1.10.0 Phase 7.
 *
 * Provides:
 * - Multiple export formats (CSV, JSON, XML, Markdown, HTML, TSV)
 * - Export options (headers, selected rows only, with statistics)
 * - Custom field selection (choose which columns to export)
 * - Template-based exports with custom formatting
 * - Batch operations (export multiple ranges/filters)
 * - Preview of first N rows before export
 */
class ExportDialog : public QDialog
{
    Q_OBJECT

  public:
    explicit ExportDialog(db::EventsContainer& events, EventsTableView* eventsView, QWidget* parent = nullptr);

  private slots:
    void OnFormatChanged(int index);
    void OnSelectFields();
    void OnBrowse();
    void OnPreview();
    void OnExport();

  private:
    enum ExportFormat
    {
        CSV,
        JSON,
        XML,
        Markdown,
        HTML,
        TSV
    };

    struct ExportOptions
    {
        ExportFormat format {CSV};
        bool includeHeaders {true};
        bool selectedRowsOnly {false};
        bool includeStatistics {false};
        bool useCustomTemplate {false};
        std::vector<int> selectedColumns;
        QString customTemplate;
        int maxRowsPreview {100};
    };

    void BuildLayout();
    void UpdateFormatDescription();
    QString GeneratePreview();

    db::EventsContainer& m_events;
    EventsTableView* m_eventsView;
    ExportOptions m_options;

    QComboBox* m_formatCombo {nullptr};
    QLineEdit* m_pathEdit {nullptr};
    QCheckBox* m_headersCheckbox {nullptr};
    QCheckBox* m_selectedOnlyCheckbox {nullptr};
    QCheckBox* m_statisticsCheckbox {nullptr};
    QCheckBox* m_customTemplateCheckbox {nullptr};
    QPlainTextEdit* m_templateEdit {nullptr};
    QPlainTextEdit* m_previewEdit {nullptr};
    QPushButton* m_selectFieldsBtn {nullptr};
    QPushButton* m_browseBtn {nullptr};
    QPushButton* m_previewBtn {nullptr};
    QPushButton* m_exportBtn {nullptr};
    QPushButton* m_cancelBtn {nullptr};
};

} // namespace ui::qt
