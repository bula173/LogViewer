#include "ResultsWidget.hpp"

#include <QHeaderView>
#include <QAbstractItemView>
#include <QTableWidgetItem>
#include <QVBoxLayout>

ResultsWidget::ResultsWidget(QWidget* parent)
    : QWidget(parent)
{
    auto* layout = new QVBoxLayout(this);
    m_table = new QTableWidget(this);
    m_table->setColumnCount(4);
    m_table->setHorizontalHeaderLabels({"Index", "Score", "Reason", "Message"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    m_table->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_table, &QTableWidget::cellClicked, this, [this](int row, int) {
        auto* item = m_table->item(row, 0);
        if (!item)
            return;
        bool ok = false;
        const int index = item->text().toInt(&ok);
        if (ok)
        {
            emit EventActivated(index);
        }
    });
    connect(m_table, &QTableWidget::cellDoubleClicked, this, [this](int row, int) {
        auto* item = m_table->item(row, 0);
        if (!item)
            return;
        bool ok = false;
        const int index = item->text().toInt(&ok);
        if (ok)
        {
            emit EventActivated(index);
        }
    });
    layout->addWidget(m_table);
}

void ResultsWidget::DisplayResults(const std::vector<MetricsEngine::Result>& results)
{
    m_table->setRowCount(static_cast<int>(results.size()));
    for (int row = 0; row < static_cast<int>(results.size()); ++row)
    {
        const auto& r = results[static_cast<size_t>(row)];
        m_table->setItem(row, 0, new QTableWidgetItem(QString::number(static_cast<qlonglong>(r.index))));
        m_table->setItem(row, 1, new QTableWidgetItem(QString::number(r.score, 'f', 3)));
        m_table->setItem(row, 2, new QTableWidgetItem(QString::fromStdString(r.reason)));
        m_table->setItem(row, 3, new QTableWidgetItem(QString::fromStdString(r.message)));
    }
}
