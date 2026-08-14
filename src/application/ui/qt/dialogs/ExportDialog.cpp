#include "ExportDialog.hpp"

#include "Logger.hpp"
#include "../events/EventsTableView.hpp"
#include "../events/EventsTableModel.hpp"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGroupBox>
#include <QComboBox>
#include <QPushButton>
#include <QCheckBox>
#include <QLineEdit>
#include <QLabel>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QFileDialog>
#include <QMessageBox>
#include <QHeaderView>

#include <fstream>
#include <sstream>
#include <iomanip>
#include <filesystem>

namespace ui::qt {

ExportDialog::ExportDialog(db::EventsContainer& events, EventsTableView* eventsView, QWidget* parent)
    : QDialog(parent), m_events(events), m_eventsView(eventsView)
{
    setWindowTitle(tr("Enhanced Export - v1.10.0"));
    setMinimumWidth(700);
    setMinimumHeight(600);
    BuildLayout();
}

void ExportDialog::BuildLayout()
{
    auto* mainLayout = new QVBoxLayout(this);

    // ── Format selection ──────────────────────────────────────────────────
    auto* formatGroup = new QGroupBox(tr("Export Format"), this);
    auto* formatLayout = new QHBoxLayout(formatGroup);
    formatLayout->addWidget(new QLabel(tr("Format:"), this));
    m_formatCombo = new QComboBox(this);
    m_formatCombo->addItem("CSV (Comma-Separated)");
    m_formatCombo->addItem("JSON (Structured)");
    m_formatCombo->addItem("XML (Hierarchical)");
    m_formatCombo->addItem("Markdown (Table)");
    m_formatCombo->addItem("HTML (Web-ready)");
    m_formatCombo->addItem("TSV (Tab-Separated)");
    connect(m_formatCombo, QOverload<int>::of(&QComboBox::currentIndexChanged),
            this, &ExportDialog::OnFormatChanged);
    formatLayout->addWidget(m_formatCombo, 1);
    mainLayout->addWidget(formatGroup);

    // ── File selection ────────────────────────────────────────────────────
    auto* fileGroup = new QGroupBox(tr("Output File"), this);
    auto* fileLayout = new QHBoxLayout(fileGroup);
    m_pathEdit = new QLineEdit(this);
    m_pathEdit->setPlaceholderText(tr("Click Browse to select output file"));
    fileLayout->addWidget(m_pathEdit, 1);
    m_browseBtn = new QPushButton(tr("Browse..."), this);
    connect(m_browseBtn, &QPushButton::clicked, this, &ExportDialog::OnBrowse);
    fileLayout->addWidget(m_browseBtn);
    mainLayout->addWidget(fileGroup);

    // ── Export options ────────────────────────────────────────────────────
    auto* optionsGroup = new QGroupBox(tr("Export Options"), this);
    auto* optionsLayout = new QVBoxLayout(optionsGroup);

    m_headersCheckbox = new QCheckBox(tr("Include column headers"), this);
    m_headersCheckbox->setChecked(true);
    optionsLayout->addWidget(m_headersCheckbox);

    m_selectedOnlyCheckbox = new QCheckBox(tr("Export selected rows only"), this);
    optionsLayout->addWidget(m_selectedOnlyCheckbox);

    m_statisticsCheckbox = new QCheckBox(tr("Include summary statistics"), this);
    optionsLayout->addWidget(m_statisticsCheckbox);

    auto* fieldLayout = new QHBoxLayout();
    m_selectFieldsBtn = new QPushButton(tr("Select Fields..."), this);
    m_selectFieldsBtn->setToolTip(tr("Choose which columns to export"));
    connect(m_selectFieldsBtn, &QPushButton::clicked, this, &ExportDialog::OnSelectFields);
    fieldLayout->addWidget(m_selectFieldsBtn);
    fieldLayout->addStretch();
    optionsLayout->addLayout(fieldLayout);

    mainLayout->addWidget(optionsGroup);

    // ── Custom template ───────────────────────────────────────────────────
    auto* templateGroup = new QGroupBox(tr("Custom Template (Optional)"), this);
    auto* templateLayout = new QVBoxLayout(templateGroup);

    m_customTemplateCheckbox = new QCheckBox(tr("Use custom template for formatting"), this);
    templateLayout->addWidget(m_customTemplateCheckbox);

    m_templateEdit = new QPlainTextEdit(this);
    m_templateEdit->setPlaceholderText(
        tr("Template variables: {TIMESTAMP}, {LEVEL}, {MESSAGE}, {ROW_NUM}, etc.\n"
           "Example: [{TIMESTAMP}] {LEVEL}: {MESSAGE}"));
    m_templateEdit->setMaximumHeight(100);
    m_templateEdit->setEnabled(false);
    connect(m_customTemplateCheckbox, &QCheckBox::toggled, m_templateEdit, &QPlainTextEdit::setEnabled);
    templateLayout->addWidget(m_templateEdit);

    mainLayout->addWidget(templateGroup);

    // ── Preview ───────────────────────────────────────────────────────────
    auto* previewGroup = new QGroupBox(tr("Preview"), this);
    auto* previewLayout = new QVBoxLayout(previewGroup);

    m_previewEdit = new QPlainTextEdit(this);
    m_previewEdit->setReadOnly(true);
    m_previewEdit->setMaximumHeight(150);
    previewLayout->addWidget(m_previewEdit);

    m_previewBtn = new QPushButton(tr("Refresh Preview"), this);
    connect(m_previewBtn, &QPushButton::clicked, this, &ExportDialog::OnPreview);
    previewLayout->addWidget(m_previewBtn);

    mainLayout->addWidget(previewGroup);

    // ── Buttons ───────────────────────────────────────────────────────────
    auto* btnLayout = new QHBoxLayout();
    m_exportBtn = new QPushButton(tr("Export"), this);
    m_exportBtn->setDefault(true);
    connect(m_exportBtn, &QPushButton::clicked, this, &ExportDialog::OnExport);
    btnLayout->addStretch();
    btnLayout->addWidget(m_exportBtn);

    m_cancelBtn = new QPushButton(tr("Cancel"), this);
    connect(m_cancelBtn, &QPushButton::clicked, this, &QDialog::reject);
    btnLayout->addWidget(m_cancelBtn);

    mainLayout->addLayout(btnLayout);

    setLayout(mainLayout);
}

void ExportDialog::OnFormatChanged(int index)
{
    m_options.format = static_cast<ExportFormat>(index);
    UpdateFormatDescription();

    // Update file extension in path
    if (!m_pathEdit->text().isEmpty())
    {
        QString path = m_pathEdit->text();
        int lastDot = path.lastIndexOf('.');
        if (lastDot != -1)
            path = path.left(lastDot);

        switch (m_options.format)
        {
            case ExportFormat::CSV: path += ".csv"; break;
            case ExportFormat::JSON: path += ".json"; break;
            case ExportFormat::XML: path += ".xml"; break;
            case ExportFormat::Markdown: path += ".md"; break;
            case ExportFormat::HTML: path += ".html"; break;
            case ExportFormat::TSV: path += ".tsv"; break;
        }
        m_pathEdit->setText(path);
    }
}

void ExportDialog::UpdateFormatDescription()
{
    QString desc;
    switch (m_options.format)
    {
        case ExportFormat::CSV:
            desc = tr("Standard CSV format, compatible with Excel, Sheets, and databases");
            break;
        case ExportFormat::JSON:
            desc = tr("JSON array of objects - excellent for data analysis and APIs");
            break;
        case ExportFormat::XML:
            desc = tr("XML format - good for structured data and interoperability");
            break;
        case ExportFormat::Markdown:
            desc = tr("Markdown table - perfect for documentation and reports");
            break;
        case ExportFormat::HTML:
            desc = tr("Standalone HTML - ready to view in browser with styling");
            break;
        case ExportFormat::TSV:
            desc = tr("Tab-separated - lightweight alternative to CSV");
            break;
    }
    util::Logger::Debug("[ExportDialog] Format: {}", desc.toStdString());
}

void ExportDialog::OnSelectFields()
{
    util::Logger::Debug("[ExportDialog] Field selection not yet implemented");
    QMessageBox::information(this, tr("Fields"), tr("Field selection coming in v1.10.1"));
}

void ExportDialog::OnBrowse()
{
    QString defaultName;
    switch (m_options.format)
    {
        case ExportFormat::CSV: defaultName = "export.csv"; break;
        case ExportFormat::JSON: defaultName = "export.json"; break;
        case ExportFormat::XML: defaultName = "export.xml"; break;
        case ExportFormat::Markdown: defaultName = "export.md"; break;
        case ExportFormat::HTML: defaultName = "export.html"; break;
        case ExportFormat::TSV: defaultName = "export.tsv"; break;
    }

    QString fileName = QFileDialog::getSaveFileName(this,
        tr("Export As"), defaultName, tr("All Files (*)"));

    if (!fileName.isEmpty())
        m_pathEdit->setText(fileName);
}

void ExportDialog::OnPreview()
{
    QString preview = GeneratePreview();
    m_previewEdit->setPlainText(preview);
}

QString ExportDialog::GeneratePreview()
{
    if (!m_eventsView || !m_eventsView->model())
        return tr("No data available for preview");

    auto* model = qobject_cast<QAbstractItemModel*>(m_eventsView->model());
    if (!model)
        return tr("Model not available");

    QString preview;
    int rowCount = (std::min)(5, model->rowCount());

    // CSV preview
    if (m_options.format == ExportFormat::CSV || m_options.format == ExportFormat::TSV)
    {
        char sep = (m_options.format == ExportFormat::CSV) ? ',' : '\t';

        // Headers
        if (m_headersCheckbox->isChecked())
        {
            for (int col = 0; col < model->columnCount(); ++col)
            {
                if (col > 0) preview += sep;
                preview += model->headerData(col, Qt::Horizontal).toString();
            }
            preview += '\n';
        }

        // Rows
        for (int row = 0; row < rowCount; ++row)
        {
            for (int col = 0; col < model->columnCount(); ++col)
            {
                if (col > 0) preview += sep;
                QString val = model->index(row, col).data().toString();
                // Quote if contains separator or newline
                if (val.contains(sep) || val.contains('\n') || val.contains('"'))
                {
                    val.replace('"', "\"\"");
                    preview += '"' + val + '"';
                }
                else
                {
                    preview += val;
                }
            }
            preview += '\n';
        }
    }
    // JSON preview
    else if (m_options.format == ExportFormat::JSON)
    {
        preview = "[\n";
        for (int row = 0; row < rowCount; ++row)
        {
            preview += "  {";
            for (int col = 0; col < model->columnCount(); ++col)
            {
                if (col > 0) preview += ", ";
                QString key = model->headerData(col, Qt::Horizontal).toString();
                QString val = model->index(row, col).data().toString();
                preview += "\"" + key + "\": \"" + val.replace("\"", "\\\"") + "\"";
            }
            preview += "}";
            if (row < rowCount - 1) preview += ",";
            preview += "\n";
        }
        preview += "]";
    }
    // Markdown preview
    else if (m_options.format == ExportFormat::Markdown)
    {
        // Table header
        if (m_headersCheckbox->isChecked())
        {
            for (int col = 0; col < model->columnCount(); ++col)
            {
                preview += "| " + model->headerData(col, Qt::Horizontal).toString() + " ";
            }
            preview += "|\n";

            // Separator
            for (int col = 0; col < model->columnCount(); ++col)
                preview += "|---";
            preview += "|\n";
        }

        // Rows
        for (int row = 0; row < rowCount; ++row)
        {
            for (int col = 0; col < model->columnCount(); ++col)
            {
                preview += "| " + model->index(row, col).data().toString().replace("\n", " ") + " ";
            }
            preview += "|\n";
        }
    }
    // HTML preview
    else if (m_options.format == ExportFormat::HTML)
    {
        preview = "<table border=\"1\" cellpadding=\"4\">\n";
        if (m_headersCheckbox->isChecked())
        {
            preview += "  <tr>";
            for (int col = 0; col < model->columnCount(); ++col)
                preview += "<th>" + model->headerData(col, Qt::Horizontal).toString() + "</th>";
            preview += "</tr>\n";
        }
        for (int row = 0; row < rowCount; ++row)
        {
            preview += "  <tr>";
            for (int col = 0; col < model->columnCount(); ++col)
                preview += "<td>" + model->index(row, col).data().toString() + "</td>";
            preview += "</tr>\n";
        }
        preview += "</table>";
    }

    if (rowCount < model->rowCount())
        preview += QString("\n... showing first %1 of %2 rows").arg(rowCount).arg(model->rowCount());

    return preview;
}

namespace {
// Escapes the characters that are special in both XML and HTML text/attribute
// content. Without this, a field containing '&lt;', '&amp;', or '"' produces
// malformed markup that downstream tools may fail to parse.
std::string EscapeMarkup(const std::string& text)
{
    std::string out;
    out.reserve(text.size());
    for (char c : text)
    {
        switch (c)
        {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            default:   out += c;        break;
        }
    }
    return out;
}
} // namespace

void ExportDialog::OnExport()
{
    if (m_pathEdit->text().isEmpty())
    {
        QMessageBox::warning(this, tr("Export"), tr("Please specify an output file"));
        return;
    }

    try
    {
        std::ofstream file(std::filesystem::path(m_pathEdit->text().toStdString()));
        if (!file.is_open())
        {
            QMessageBox::critical(this, tr("Export Failed"), tr("Could not open file for writing"));
            return;
        }

        auto* model = qobject_cast<QAbstractItemModel*>(m_eventsView->model());
        if (!model)
        {
            QMessageBox::critical(this, tr("Export Failed"), tr("No data available"));
            return;
        }

        // Determine which rows to export
        std::vector<int> rowsToExport;
        if (m_selectedOnlyCheckbox->isChecked())
        {
            // Export selected rows (would need selection model)
            util::Logger::Warn("[ExportDialog] Selected rows export not yet implemented");
            for (int i = 0; i < model->rowCount(); ++i)
                rowsToExport.push_back(i);
        }
        else
        {
            for (int i = 0; i < model->rowCount(); ++i)
                rowsToExport.push_back(i);
        }

        // Export based on format
        switch (m_options.format)
        {
            case ExportFormat::CSV:
            case ExportFormat::TSV:
            {
                char sep = (m_options.format == ExportFormat::CSV) ? ',' : '\t';
                if (m_headersCheckbox->isChecked())
                {
                    for (int col = 0; col < model->columnCount(); ++col)
                    {
                        if (col > 0) file << sep;
                        QString val = model->headerData(col, Qt::Horizontal).toString();
                        file << val.toStdString();
                    }
                    file << '\n';
                }

                for (int row : rowsToExport)
                {
                    for (int col = 0; col < model->columnCount(); ++col)
                    {
                        if (col > 0) file << sep;
                        QString val = model->index(row, col).data().toString();
                        file << val.toStdString();
                    }
                    file << '\n';
                }
                break;
            }

            case ExportFormat::JSON:
            {
                file << "[\n";
                for (size_t i = 0; i < rowsToExport.size(); ++i)
                {
                    int row = rowsToExport[i];
                    file << "  {";
                    for (int col = 0; col < model->columnCount(); ++col)
                    {
                        if (col > 0) file << ", ";
                        QString key = model->headerData(col, Qt::Horizontal).toString();
                        QString val = model->index(row, col).data().toString();
                        file << "\"" << key.toStdString() << "\": \""
                             << val.replace("\"", "\\\"").toStdString() << "\"";
                    }
                    file << "}";
                    if (i < rowsToExport.size() - 1) file << ",";
                    file << "\n";
                }
                file << "]";
                break;
            }

            case ExportFormat::Markdown:
            {
                if (m_headersCheckbox->isChecked())
                {
                    for (int col = 0; col < model->columnCount(); ++col)
                        file << "| " << model->headerData(col, Qt::Horizontal).toString().toStdString() << " ";
                    file << "|\n";

                    for (int col = 0; col < model->columnCount(); ++col)
                        file << "|---";
                    file << "|\n";
                }

                for (int row : rowsToExport)
                {
                    for (int col = 0; col < model->columnCount(); ++col)
                    {
                        QString val = model->index(row, col).data().toString();
                        file << "| " << val.replace("\n", " ").toStdString() << " ";
                    }
                    file << "|\n";
                }
                break;
            }

            case ExportFormat::HTML:
            {
                file << "<!DOCTYPE html>\n<html>\n<body>\n";
                file << "<table border=\"1\" cellpadding=\"4\">\n";

                if (m_headersCheckbox->isChecked())
                {
                    file << "  <tr>";
                    for (int col = 0; col < model->columnCount(); ++col)
                        file << "<th>" << EscapeMarkup(model->headerData(col, Qt::Horizontal).toString().toStdString()) << "</th>";
                    file << "</tr>\n";
                }

                for (int row : rowsToExport)
                {
                    file << "  <tr>";
                    for (int col = 0; col < model->columnCount(); ++col)
                        file << "<td>" << EscapeMarkup(model->index(row, col).data().toString().toStdString()) << "</td>";
                    file << "</tr>\n";
                }

                file << "</table>\n</body>\n</html>";
                break;
            }

            case ExportFormat::XML:
            {
                file << "<?xml version=\"1.0\"?>\n<events>\n";
                for (int row : rowsToExport)
                {
                    file << "  <event";
                    for (int col = 0; col < model->columnCount(); ++col)
                    {
                        QString key = model->headerData(col, Qt::Horizontal).toString();
                        QString val = model->index(row, col).data().toString();
                        file << " " << EscapeMarkup(key.toStdString()) << "=\"" << EscapeMarkup(val.toStdString()) << "\"";
                    }
                    file << " />\n";
                }
                file << "</events>";
                break;
            }
        }

        const bool writeOk = !file.fail();
        file.close();
        if (!writeOk || file.fail())
        {
            util::Logger::Error("[ExportDialog] Export failed: write error to {}",
                m_pathEdit->text().toStdString());
            QMessageBox::critical(this, tr("Export Failed"),
                tr("An error occurred while writing to\n%1").arg(m_pathEdit->text()));
            return;
        }

        util::Logger::Info("[ExportDialog] Successfully exported {} rows", rowsToExport.size());
        QMessageBox::information(this, tr("Export Complete"),
            tr("Successfully exported %1 rows to\n%2")
                .arg(rowsToExport.size())
                .arg(m_pathEdit->text()));

        accept();
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[ExportDialog] Export failed: {}", e.what());
        QMessageBox::critical(this, tr("Export Failed"), tr("Error: %1").arg(e.what()));
    }
}

} // namespace ui::qt
