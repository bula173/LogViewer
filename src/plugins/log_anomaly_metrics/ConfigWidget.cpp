#include "ConfigWidget.hpp"

#include "MetricsEngine.hpp"

#include <QDialog>
#include <QFileDialog>
#include <QHeaderView>
#include <QHBoxLayout>
#include <QLabel>
#include <QAbstractItemView>
#include <QPushButton>
#include <QSignalBlocker>
#include <QTableWidget>
#include <QVBoxLayout>
#include <boost/regex.hpp>

ConfigWidget::ConfigWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    layout->addWidget(new QLabel("Metric configuration", this));

    m_table = new QTableWidget(this);
    m_table->setColumnCount(6);
    m_table->setHorizontalHeaderLabels({"Enabled", "Name", "Field", "Pattern (regex)", "Weight", "Min severity"});
    m_table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(3, QHeaderView::Stretch);
    m_table->horizontalHeader()->setSectionResizeMode(4, QHeaderView::ResizeToContents);
    m_table->horizontalHeader()->setSectionResizeMode(5, QHeaderView::ResizeToContents);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    layout->addWidget(m_table);

    auto* rowButtons = new QHBoxLayout();
    auto* addBtn = new QPushButton("Add rule", this);
    auto* removeBtn = new QPushButton("Remove selected", this);
    rowButtons->addWidget(addBtn);
    rowButtons->addWidget(removeBtn);
    layout->addLayout(rowButtons);

    auto* fileButtons = new QHBoxLayout();
    auto* loadBtn = new QPushButton("Load JSON", this);
    auto* saveBtn = new QPushButton("Save JSON", this);
    fileButtons->addWidget(loadBtn);
    fileButtons->addWidget(saveBtn);
    layout->addLayout(fileButtons);

    connect(addBtn, &QPushButton::clicked, this, [this]() {
        addRuleRow("Rule " + QString::number(m_table->rowCount() + 1), "message", ".*", 1.0, -1, true);
        applyTableToEngine();
    });

    connect(removeBtn, &QPushButton::clicked, this, [this]() {
        removeSelectedRows();
        applyTableToEngine();
    });

    connect(m_table, &QTableWidget::cellChanged, this, [this](int, int) {
        applyTableToEngine();
    });

    connect(loadBtn, &QPushButton::clicked, this, [this]() {
        QFileDialog dialog(this, tr("Load configuration"), QString(), "JSON (*.json)");
        dialog.setFileMode(QFileDialog::ExistingFile);
        dialog.setAcceptMode(QFileDialog::AcceptOpen);
        dialog.setNameFilter("JSON (*.json)");
#ifdef __APPLE__
        // Workaround for macOS native dialog issues
        dialog.setWindowModality(Qt::WindowModal);
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
#endif
        if (dialog.exec() == QDialog::Accepted)
        {
            const auto path = dialog.selectedFiles().value(0);
            if (!path.isEmpty())
            {
                LoadFromFile(path);
            }
        }
    });

    connect(saveBtn, &QPushButton::clicked, this, [this]() {
        QFileDialog dialog(this, tr("Save configuration"), QString(), "JSON (*.json)");
        dialog.setAcceptMode(QFileDialog::AcceptSave);
        dialog.setNameFilter("JSON (*.json)");
        dialog.setDefaultSuffix("json");
#ifdef __APPLE__
        // Workaround for macOS native dialog issues
        dialog.setWindowModality(Qt::WindowModal);
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
#endif
        if (dialog.exec() == QDialog::Accepted)
        {
            const auto path = dialog.selectedFiles().value(0);
            if (!path.isEmpty())
            {
                SaveToFile(path);
            }
        }
    });
}

void ConfigWidget::SetEngine(MetricsEngine* engine)
{
    m_engine = engine;
    refreshFromEngine();
}

void ConfigWidget::LoadFromFile(const QString& path)
{
    if (m_engine && m_engine->LoadConfig(path.toStdString()))
    {
        refreshFromEngine();
        emit configChanged();
    }
}

void ConfigWidget::SaveToFile(const QString& path)
{
    if (!m_engine)
    {
        return;
    }

    applyTableToEngine();
    m_engine->SaveConfig(path.toStdString());
}

void ConfigWidget::refreshFromEngine()
{
    if (!m_engine || !m_table)
    {
        return;
    }

    const auto& rules = m_engine->GetRules();
    QSignalBlocker blocker(m_table);
    m_table->setRowCount(static_cast<int>(rules.size()));

    for (int row = 0; row < static_cast<int>(rules.size()); ++row)
    {
        const auto& rule = rules[static_cast<size_t>(row)];
        auto* enabledItem = new QTableWidgetItem();
        enabledItem->setCheckState(rule.enabled ? Qt::Checked : Qt::Unchecked);
        m_table->setItem(row, 0, enabledItem);
        m_table->setItem(row, 1, new QTableWidgetItem(QString::fromStdString(rule.name)));
        m_table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(rule.fieldKey)));
        m_table->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(rule.pattern.str())));
        m_table->setItem(row, 4, new QTableWidgetItem(QString::number(rule.weight, 'f', 3)));
        m_table->setItem(row, 5, new QTableWidgetItem(QString::number(rule.minSeverity)));
    }
}

void ConfigWidget::applyTableToEngine()
{
    if (!m_engine || !m_table)
    {
        return;
    }

    std::vector<MetricsEngine::Rule> rules;
    const int rows = m_table->rowCount();
    rules.reserve(static_cast<size_t>(rows));

    for (int row = 0; row < rows; ++row)
    {
        const auto* enabledItem = m_table->item(row, 0);
        const auto* nameItem = m_table->item(row, 1);
        const auto* fieldItem = m_table->item(row, 2);
        const auto* patternItem = m_table->item(row, 3);
        const auto* weightItem = m_table->item(row, 4);
        const auto* minSeverityItem = m_table->item(row, 5);

        MetricsEngine::Rule rule;
        rule.enabled = enabledItem ? (enabledItem->checkState() == Qt::Checked) : true;
        rule.name = nameItem ? nameItem->text().toStdString() : std::string();
        rule.fieldKey = fieldItem ? fieldItem->text().toStdString() : std::string();

        const QString patternText = patternItem && !patternItem->text().isEmpty()
            ? patternItem->text()
            : QStringLiteral(".*");
        try
        {
            rule.pattern = boost::regex(patternText.toStdString(), boost::regex::perl | boost::regex::icase);
        }
        catch (...)
        {
            continue;
        }

        bool ok = false;
        const double weight = weightItem ? weightItem->text().toDouble(&ok) : 1.0;
        rule.weight = ok ? weight : 1.0;

        ok = false;
        const int minSeverity = minSeverityItem ? minSeverityItem->text().toInt(&ok) : -1;
        rule.minSeverity = ok ? minSeverity : -1;

        rules.push_back(std::move(rule));
    }

    m_engine->SetRules(std::move(rules));
    emit configChanged();
}

void ConfigWidget::addRuleRow(const QString& name, const QString& field, const QString& pattern,
    double weight, int minSeverity, bool enabled)
{
    if (!m_table)
    {
        return;
    }

    const int row = m_table->rowCount();
    m_table->insertRow(row);
    auto* enabledItem = new QTableWidgetItem();
    enabledItem->setCheckState(enabled ? Qt::Checked : Qt::Unchecked);
    m_table->setItem(row, 0, enabledItem);
    m_table->setItem(row, 1, new QTableWidgetItem(name));
    m_table->setItem(row, 2, new QTableWidgetItem(field));
    m_table->setItem(row, 3, new QTableWidgetItem(pattern));
    m_table->setItem(row, 4, new QTableWidgetItem(QString::number(weight, 'f', 3)));
    m_table->setItem(row, 5, new QTableWidgetItem(QString::number(minSeverity)));
}

void ConfigWidget::removeSelectedRows()
{
    if (!m_table)
    {
        return;
    }

    const auto* selection = m_table->selectionModel();
    if (!selection)
    {
        return;
    }

    const auto selected = selection->selectedRows();
    for (const auto& index : selected)
    {
        m_table->removeRow(index.row());
    }
}
