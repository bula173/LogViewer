#include "StatsSummaryPanel.hpp"

#include "Config.hpp"
#include "EventsContainer.hpp"
#include "EventsTableView.hpp"

#include <QBarCategoryAxis>
#include <QBarSeries>
#include <QBarSet>
#include <QChart>
#include <QChartView>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHorizontalBarSeries>
#include <QLabel>
#include <QScrollArea>
#include <QSpinBox>
#include <QTableWidget>
#include <QValueAxis>
#include <QVBoxLayout>

#include <QDateTime>
#include <QHeaderView>

#include <algorithm>
#include <map>
#include <string>

namespace ui::qt {

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

StatsSummaryPanel::StatsSummaryPanel(db::EventsContainer& events,
                                     EventsTableView*     eventsView,
                                     QWidget*             parent)
    : QWidget(parent), m_events(events), m_eventsView(eventsView)
{
    BuildLayout();
}

void StatsSummaryPanel::BuildLayout()
{
    // Inner widget that lives inside the scroll area
    auto* inner  = new QWidget(this);
    auto* layout = new QVBoxLayout(inner);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);

    // ── Summary label ────────────────────────────────────────────────────
    m_summaryLabel = new QLabel(tr("No data loaded"), inner);
    m_summaryLabel->setAlignment(Qt::AlignCenter);
    layout->addWidget(m_summaryLabel);

    // ── Event Types section ──────────────────────────────────────────────
    auto* typeBox    = new QGroupBox(tr("Event Types"), inner);
    auto* typeLayout = new QVBoxLayout(typeBox);
    m_typeChartView  = new QChartView(typeBox);
    m_typeChartView->setRenderHint(QPainter::Antialiasing);
    m_typeChartView->setMinimumHeight(220);
    m_typeChartView->setChart(new QChart()); // placeholder
    typeLayout->addWidget(m_typeChartView);
    layout->addWidget(typeBox);

    // ── Events Over Time section ─────────────────────────────────────────
    auto* timeBox    = new QGroupBox(tr("Events Over Time"), inner);
    auto* timeLayout = new QVBoxLayout(timeBox);
    m_timeChartView  = new QChartView(timeBox);
    m_timeChartView->setRenderHint(QPainter::Antialiasing);
    m_timeChartView->setMinimumHeight(200);
    m_timeChartView->setChart(new QChart()); // placeholder
    timeLayout->addWidget(m_timeChartView);
    layout->addWidget(timeBox);

    // ── Top N Values section ─────────────────────────────────────────────
    auto* topBox    = new QGroupBox(tr("Top Values"), inner);
    auto* topLayout = new QVBoxLayout(topBox);

    auto* controlRow = new QHBoxLayout();
    m_columnCombo    = new QComboBox(topBox);
    m_columnCombo->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    controlRow->addWidget(m_columnCombo);
    controlRow->addWidget(new QLabel(tr("Top"), topBox));
    m_topNSpin = new QSpinBox(topBox);
    m_topNSpin->setRange(1, 100);
    m_topNSpin->setValue(10);
    controlRow->addWidget(m_topNSpin);
    topLayout->addLayout(controlRow);

    m_topNTable = new QTableWidget(0, 3, topBox);
    m_topNTable->setHorizontalHeaderLabels({tr("#"), tr("Value"), tr("Count")});
    m_topNTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_topNTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_topNTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
    m_topNTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_topNTable->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_topNTable->verticalHeader()->hide();
    topLayout->addWidget(m_topNTable);

    layout->addWidget(topBox);
    layout->addStretch();

    // Column combo changes trigger top-N refresh only
    connect(m_columnCombo, &QComboBox::currentIndexChanged,
            this, [this](int) { RefreshTopNTable(); });
    connect(m_topNSpin, &QSpinBox::valueChanged,
            this, [this](int) { RefreshTopNTable(); });

    // ── Scroll area wrapping everything ─────────────────────────────────
    auto* scroll = new QScrollArea(this);
    scroll->setWidgetResizable(true);
    scroll->setWidget(inner);

    auto* outerLayout = new QVBoxLayout(this);
    outerLayout->setContentsMargins(0, 0, 0, 0);
    outerLayout->addWidget(scroll);
    setLayout(outerLayout);
}

// ---------------------------------------------------------------------------
// Public slot
// ---------------------------------------------------------------------------

void StatsSummaryPanel::Refresh()
{
    // Repopulate the column combo whenever the model changes
    PopulateColumnCombo();
    RefreshTypeChart();
    RefreshTimeChart();
    RefreshTopNTable();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::vector<unsigned long> StatsSummaryPanel::VisibleIndices() const
{
    const std::vector<unsigned long>* filtered = m_eventsView->GetFilteredIndices();
    if (filtered && !filtered->empty())
        return *filtered;

    // No active filter — use all events
    const size_t total = m_events.Size();
    std::vector<unsigned long> all;
    all.reserve(total);
    for (size_t i = 0; i < total; ++i)
        all.push_back(static_cast<unsigned long>(i));
    return all;
}

void StatsSummaryPanel::PopulateColumnCombo()
{
    auto* model = m_eventsView ? m_eventsView->model() : nullptr;
    if (!model) return;

    const QString current = m_columnCombo->currentText();
    m_columnCombo->blockSignals(true);
    m_columnCombo->clear();
    const int cols = model->columnCount();
    for (int c = 0; c < cols; ++c)
        m_columnCombo->addItem(
            model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString(),
            c);

    // Restore previously selected column if still present
    const int idx = m_columnCombo->findText(current);
    m_columnCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_columnCombo->blockSignals(false);
}

// ---------------------------------------------------------------------------
// Type chart
// ---------------------------------------------------------------------------

void StatsSummaryPanel::RefreshTypeChart()
{
    const auto indices = VisibleIndices();

    m_summaryLabel->setText(tr("%1 event(s) visible").arg(indices.size()));

    const std::string& typeField = config::GetConfig().typeFilterField;

    std::map<std::string, int> counts;
    for (unsigned long idx : indices)
    {
        const std::string val = m_events.GetEvent(static_cast<int>(idx)).findByKey(typeField);
        counts[val.empty() ? "(none)" : val]++;
    }

    // Sort by count descending, cap at 15 entries
    using Pair = std::pair<std::string, int>;
    std::vector<Pair> sorted(counts.begin(), counts.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const Pair& a, const Pair& b) { return a.second > b.second; });
    if (sorted.size() > 15)
        sorted.resize(15);

    auto* barSet = new QBarSet(QString::fromStdString(typeField));
    QStringList categories;
    for (const auto& [label, count] : sorted)
    {
        *barSet << count;
        categories << QString::fromStdString(label);
    }

    auto* series = new QHorizontalBarSeries();
    series->append(barSet);
    series->setLabelsVisible(true);
    series->setLabelsPosition(QAbstractBarSeries::LabelsOutsideEnd);

    auto* chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(tr("Event Types"));
    chart->setAnimationOptions(QChart::NoAnimation);
    chart->legend()->hide();
    chart->setMargins(QMargins(4, 4, 4, 4));

    auto* axisY = new QBarCategoryAxis();
    axisY->append(categories);
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    auto* axisX = new QValueAxis();
    axisX->setLabelFormat("%d");
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    // Adjust chart height to the number of bars
    const int barHeight = 28;
    const int chartH = std::max(150, static_cast<int>(sorted.size()) * barHeight + 60);
    m_typeChartView->setMinimumHeight(chartH);

    m_typeChartView->setChart(chart);
}

// ---------------------------------------------------------------------------
// Time histogram
// ---------------------------------------------------------------------------

void StatsSummaryPanel::RefreshTimeChart()
{
    const auto indices = VisibleIndices();

    // Candidate timestamp field names (in priority order)
    static const std::vector<std::string> kCandidates {
        "timestamp", "time", "datetime", "@timestamp", "date"
    };

    // Determine which field contains parseable timestamps
    std::string tsField;

    // Try to parse a string as a QDateTime using common formats
    auto tryParse = [](const QString& s) -> QDateTime {
        // ISO 8601 variants
        QDateTime dt = QDateTime::fromString(s, Qt::ISODateWithMs);
        if (dt.isValid()) return dt;
        dt = QDateTime::fromString(s, Qt::ISODate);
        if (dt.isValid()) return dt;
        // Other common formats
        for (const char* fmt : {"yyyy-MM-dd HH:mm:ss.zzz",
                                "yyyy-MM-dd HH:mm:ss",
                                "dd/MMM/yyyy:HH:mm:ss"}) {
            dt = QDateTime::fromString(s, QString::fromLatin1(fmt));
            if (dt.isValid()) return dt;
        }
        // Unix epoch (seconds)
        bool ok = false;
        const qint64 epoch = s.toLongLong(&ok);
        if (ok) return QDateTime::fromSecsSinceEpoch(epoch);
        return {};
    };

    // Detect field from the first parseable event
    for (const auto& candidate : kCandidates)
    {
        for (unsigned long idx : indices)
        {
            const std::string val =
                m_events.GetEvent(static_cast<int>(idx)).findByKey(candidate);
            if (!val.empty() && tryParse(QString::fromStdString(val)).isValid())
            {
                tsField = candidate;
                break;
            }
        }
        if (!tsField.empty()) break;
    }

    auto* chart = new QChart();
    chart->setTitle(tr("Events Over Time"));
    chart->setAnimationOptions(QChart::NoAnimation);
    chart->legend()->hide();
    chart->setMargins(QMargins(4, 4, 4, 4));

    if (tsField.empty() || indices.empty())
    {
        // No parseable timestamps — show empty chart with message
        chart->setTitle(tr("Events Over Time (no timestamp field found)"));
        m_timeChartView->setChart(chart);
        return;
    }

    // Collect valid timestamps
    std::vector<QDateTime> times;
    times.reserve(indices.size());
    for (unsigned long idx : indices)
    {
        const QString val = QString::fromStdString(
            m_events.GetEvent(static_cast<int>(idx)).findByKey(tsField));
        const QDateTime dt = tryParse(val);
        if (dt.isValid()) times.push_back(dt);
    }

    if (times.size() < 2)
    {
        chart->setTitle(tr("Events Over Time (insufficient timestamp data)"));
        m_timeChartView->setChart(chart);
        return;
    }

    const QDateTime tMin = *std::min_element(times.begin(), times.end());
    const QDateTime tMax = *std::max_element(times.begin(), times.end());
    const qint64 spanSecs = tMin.secsTo(tMax);

    // Determine bucket count and label format
    const int buckets = std::min(20, static_cast<int>(times.size()));
    const qint64 bucketSecs = std::max(qint64(1), spanSecs / buckets);

    QString labelFmt = spanSecs > 86400 ? "MM-dd HH:mm" : "HH:mm:ss";

    std::vector<int> bucketCounts(static_cast<size_t>(buckets), 0);
    for (const QDateTime& dt : times)
    {
        qint64 offset = tMin.secsTo(dt);
        int b = static_cast<int>(offset / bucketSecs);
        if (b >= buckets) b = buckets - 1;
        bucketCounts[static_cast<size_t>(b)]++;
    }

    auto* barSet = new QBarSet("count");
    QStringList categories;
    for (int i = 0; i < buckets; ++i)
    {
        *barSet << bucketCounts[static_cast<size_t>(i)];
        categories << tMin.addSecs(i * bucketSecs).toString(labelFmt);
    }

    auto* series = new QBarSeries();
    series->append(barSet);
    series->setLabelsVisible(false);

    chart->addSeries(series);

    auto* axisX = new QBarCategoryAxis();
    axisX->append(categories);
    axisX->setLabelsAngle(-45);
    chart->addAxis(axisX, Qt::AlignBottom);
    series->attachAxis(axisX);

    auto* axisY = new QValueAxis();
    axisY->setLabelFormat("%d");
    chart->addAxis(axisY, Qt::AlignLeft);
    series->attachAxis(axisY);

    m_timeChartView->setChart(chart);
}

// ---------------------------------------------------------------------------
// Top-N table
// ---------------------------------------------------------------------------

void StatsSummaryPanel::RefreshTopNTable()
{
    auto* model = m_eventsView ? m_eventsView->model() : nullptr;
    if (!model || m_columnCombo->count() == 0)
    {
        m_topNTable->setRowCount(0);
        return;
    }

    const int col  = m_columnCombo->currentData().toInt();
    const int topN = m_topNSpin->value();
    const auto indices = VisibleIndices();

    std::map<QString, int> counts;
    for (unsigned long idx : indices)
    {
        const QString val = model->data(
            model->index(static_cast<int>(idx), col), Qt::DisplayRole).toString();
        counts[val]++;
    }

    // Sort descending by count
    using Pair = std::pair<QString, int>;
    std::vector<Pair> sorted(counts.begin(), counts.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const Pair& a, const Pair& b) { return a.second > b.second; });
    if (static_cast<int>(sorted.size()) > topN)
        sorted.resize(static_cast<size_t>(topN));

    m_topNTable->setRowCount(static_cast<int>(sorted.size()));
    for (int r = 0; r < static_cast<int>(sorted.size()); ++r)
    {
        m_topNTable->setItem(r, 0, new QTableWidgetItem(QString::number(r + 1)));
        m_topNTable->setItem(r, 1, new QTableWidgetItem(sorted[static_cast<size_t>(r)].first));
        m_topNTable->setItem(r, 2,
            new QTableWidgetItem(QString::number(sorted[static_cast<size_t>(r)].second)));
    }
}

} // namespace ui::qt
