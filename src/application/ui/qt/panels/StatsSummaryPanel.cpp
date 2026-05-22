#include "StatsSummaryPanel.hpp"

#include "CanStatisticsStrategy.hpp"
#include "GenericStatisticsStrategy.hpp"
#include "Config.hpp"
#include "utils/PanelUtils.hpp"

#include <QBarCategoryAxis>
#include <QBarSet>
#include <QBrush>
#include <QChart>
#include <QChartView>
#include <QColor>
#include <QCursor>
#include <QDateTime>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QHorizontalBarSeries>
#include <QLabel>
#include <QScrollArea>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QToolTip>
#include <QValueAxis>
#include <QVBoxLayout>

#include <algorithm>
#include <map>
#include <set>
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
    auto* inner  = new QWidget(this);
    auto* layout = new QVBoxLayout(inner);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(8);

    // ── Format Statistics (strategy-driven, hidden for non-CAN formats) ──
    {
        m_formatStatsGroup = new QGroupBox(tr("Format Statistics"), inner);
        auto* fl = new QVBoxLayout(m_formatStatsGroup);
        m_formatStatsTable = new QTableWidget(0, 2, m_formatStatsGroup);
        m_formatStatsTable->horizontalHeader()->hide();
        m_formatStatsTable->verticalHeader()->hide();
        m_formatStatsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_formatStatsTable->setSelectionMode(QAbstractItemView::NoSelection);
        m_formatStatsTable->horizontalHeader()->setSectionResizeMode(
            0, QHeaderView::ResizeToContents);
        m_formatStatsTable->horizontalHeader()->setSectionResizeMode(
            1, QHeaderView::Stretch);
        m_formatStatsTable->setShowGrid(false);
        m_formatStatsTable->setAlternatingRowColors(true);
        fl->addWidget(m_formatStatsTable);
        m_formatStatsGroup->setVisible(false);
        layout->addWidget(m_formatStatsGroup);
    }

    // ── Summary table ─────────────────────────────────────────────────────
    {
        auto* box = new QGroupBox(tr("Summary"), inner);
        auto* bl  = new QVBoxLayout(box);
        m_summaryTable = new QTableWidget(0, 2, box);
        m_summaryTable->horizontalHeader()->hide();
        m_summaryTable->verticalHeader()->hide();
        m_summaryTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_summaryTable->setSelectionMode(QAbstractItemView::NoSelection);
        m_summaryTable->horizontalHeader()->setSectionResizeMode(
            0, QHeaderView::ResizeToContents);
        m_summaryTable->horizontalHeader()->setSectionResizeMode(
            1, QHeaderView::Stretch);
        m_summaryTable->setShowGrid(false);
        m_summaryTable->setAlternatingRowColors(true);
        bl->addWidget(m_summaryTable);
        layout->addWidget(box);
    }

    // ── Event Types section ───────────────────────────────────────────────
    {
        auto* typeBox    = new QGroupBox(tr("Event Types"), inner);
        auto* typeLayout = new QVBoxLayout(typeBox);
        m_typeChartView  = new QChartView(typeBox);
        m_typeChartView->setRenderHint(QPainter::Antialiasing);
        m_typeChartView->setMinimumHeight(220);
        m_typeChartView->setChart(new QChart());
        typeLayout->addWidget(m_typeChartView);
        layout->addWidget(typeBox);
    }

    // ── Top N Values section ──────────────────────────────────────────────
    {
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

        // Four columns: rank, value, count, percentage share
        m_topNTable = new QTableWidget(0, 4, topBox);
        m_topNTable->setHorizontalHeaderLabels({tr("#"), tr("Value"), tr("Count"), tr("%")});
        m_topNTable->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
        m_topNTable->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
        m_topNTable->horizontalHeader()->setSectionResizeMode(2, QHeaderView::ResizeToContents);
        m_topNTable->horizontalHeader()->setSectionResizeMode(3, QHeaderView::ResizeToContents);
        m_topNTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_topNTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_topNTable->verticalHeader()->hide();
        topLayout->addWidget(m_topNTable);
        layout->addWidget(topBox);
    }

    // ── Field Statistics section ──────────────────────────────────────────
    {
        auto* fieldBox    = new QGroupBox(tr("Field Statistics"), inner);
        auto* fieldLayout = new QVBoxLayout(fieldBox);
        m_fieldStatsTable = new QTableWidget(0, 3, fieldBox);
        m_fieldStatsTable->setHorizontalHeaderLabels(
            {tr("Field"), tr("Unique values"), tr("Fill %")});
        m_fieldStatsTable->horizontalHeader()->setSectionResizeMode(
            0, QHeaderView::Stretch);
        m_fieldStatsTable->horizontalHeader()->setSectionResizeMode(
            1, QHeaderView::ResizeToContents);
        m_fieldStatsTable->horizontalHeader()->setSectionResizeMode(
            2, QHeaderView::ResizeToContents);
        m_fieldStatsTable->setEditTriggers(QAbstractItemView::NoEditTriggers);
        m_fieldStatsTable->setSelectionBehavior(QAbstractItemView::SelectRows);
        m_fieldStatsTable->verticalHeader()->hide();
        m_fieldStatsTable->setAlternatingRowColors(true);
        fieldLayout->addWidget(m_fieldStatsTable);
        layout->addWidget(fieldBox);
    }

    layout->addStretch();

    // Column combo / spin box changes only re-draw the top-N table
    connect(m_columnCombo, &QComboBox::currentIndexChanged, this, [this](int) {
        RefreshTopNTable(VisibleIndices());
    });
    connect(m_topNSpin, &QSpinBox::valueChanged, this, [this](int) {
        RefreshTopNTable(VisibleIndices());
    });

    // ── Scroll area wrapping everything ───────────────────────────────────
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
    PopulateColumnCombo();
    const auto indices = VisibleIndices();   // compute once, share everywhere
    RefreshFormatStats(indices);
    RefreshSummaryTable(indices);
    RefreshTypeChart(indices);
    RefreshTopNTable(indices);
    RefreshFieldStats(indices);
}

// ---------------------------------------------------------------------------
// Strategy selection and format-specific stats
// ---------------------------------------------------------------------------

std::unique_ptr<IStatisticsStrategy> StatsSummaryPanel::SelectStrategy() const
{
    if (CanStatisticsStrategy{}.Matches(m_events))
        return std::make_unique<CanStatisticsStrategy>();
    return std::make_unique<GenericStatisticsStrategy>();
}

void StatsSummaryPanel::RefreshFormatStats(const std::vector<unsigned long>& indices)
{
    const auto sections = SelectStrategy()->Compute(m_events, indices);

    m_formatStatsTable->setRowCount(0);

    if (sections.empty())
    {
        m_formatStatsGroup->setVisible(false);
        return;
    }

    m_formatStatsGroup->setVisible(true);

    for (const auto& section : sections)
    {
        // Bold full-width section header row
        const int hr = m_formatStatsTable->rowCount();
        m_formatStatsTable->insertRow(hr);
        auto* hItem = new QTableWidgetItem(QString::fromStdString(section.title));
        QFont bold = hItem->font();
        bold.setBold(true);
        hItem->setFont(bold);
        hItem->setBackground(QBrush(palette().mid()));
        hItem->setFlags(hItem->flags() & ~Qt::ItemIsEditable);
        m_formatStatsTable->setItem(hr, 0, hItem);
        m_formatStatsTable->setSpan(hr, 0, 1, 2);

        for (const auto& row : section.rows)
        {
            const int r = m_formatStatsTable->rowCount();
            m_formatStatsTable->insertRow(r);

            auto* labelItem = new QTableWidgetItem(QString::fromStdString(row.label));
            auto* valueItem = new QTableWidgetItem(QString::fromStdString(row.value));

            QFont lf = labelItem->font();
            lf.setBold(true);
            labelItem->setFont(lf);
            labelItem->setFlags(labelItem->flags() & ~Qt::ItemIsEditable);
            valueItem->setFlags(valueItem->flags() & ~Qt::ItemIsEditable);

            m_formatStatsTable->setItem(r, 0, labelItem);
            m_formatStatsTable->setItem(r, 1, valueItem);
        }
    }

    m_formatStatsTable->resizeRowsToContents();
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::vector<unsigned long> StatsSummaryPanel::VisibleIndices() const
{
    const std::vector<unsigned long>* filtered = m_eventsView->GetFilteredIndices();
    if (filtered && !filtered->empty())
        return *filtered;

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
            model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString(), c);

    const int idx = m_columnCombo->findText(current);
    m_columnCombo->setCurrentIndex(idx >= 0 ? idx : 0);
    m_columnCombo->blockSignals(false);
}

// ---------------------------------------------------------------------------
// Static helpers
// ---------------------------------------------------------------------------

std::string StatsSummaryPanel::DetectTimestampField(
    db::EventsContainer&              events,
    const std::vector<unsigned long>& indices)
{
    for (const auto& candidate : panel_utils::kTsFields)
    {
        for (unsigned long idx : indices)
        {
            const std::string val =
                events.GetEvent(static_cast<int>(idx)).findByKey(candidate);
            if (!val.empty() &&
                panel_utils::ParseTimestamp(QString::fromStdString(val)).isValid())
                return candidate;
        }
    }
    return {};
}

// ---------------------------------------------------------------------------
// Summary table
// ---------------------------------------------------------------------------

void StatsSummaryPanel::RefreshSummaryTable(
    const std::vector<unsigned long>& indices)
{
    const size_t totalEvents   = m_events.Size();
    const size_t visibleEvents = indices.size();

    // Count unique type values in the visible set
    const std::string& typeField = config::GetConfig().typeFilterField;
    std::set<std::string> uniqueTypes;
    for (unsigned long idx : indices)
    {
        const std::string v =
            m_events.GetEvent(static_cast<int>(idx)).findByKey(typeField);
        if (!v.empty()) uniqueTypes.insert(v);
    }

    // Detect timestamp field and compute time range
    const std::string tsField = DetectTimestampField(m_events, indices);
    QDateTime tMin, tMax;
    if (!tsField.empty())
    {
        for (unsigned long idx : indices)
        {
            const QDateTime dt = panel_utils::ParseTimestamp(QString::fromStdString(
                m_events.GetEvent(static_cast<int>(idx)).findByKey(tsField)));
            if (!dt.isValid()) continue;
            if (!tMin.isValid() || dt < tMin) tMin = dt;
            if (!tMax.isValid() || dt > tMax) tMax = dt;
        }
    }

    // Helper: append one property row
    m_summaryTable->setRowCount(0);
    auto addRow = [this](const QString& label, const QString& value) {
        const int r = m_summaryTable->rowCount();
        m_summaryTable->insertRow(r);
        auto* lItem = new QTableWidgetItem(label);
        auto* vItem = new QTableWidgetItem(value);
        lItem->setFlags(lItem->flags() & ~Qt::ItemIsEditable);
        vItem->setFlags(vItem->flags() & ~Qt::ItemIsEditable);
        QFont bold = lItem->font();
        bold.setBold(true);
        lItem->setFont(bold);
        m_summaryTable->setItem(r, 0, lItem);
        m_summaryTable->setItem(r, 1, vItem);
    };

    addRow(tr("Total loaded"), QString::number(static_cast<qulonglong>(totalEvents)));

    const double visPct = totalEvents > 0
        ? 100.0 * static_cast<double>(visibleEvents) / static_cast<double>(totalEvents)
        : 0.0;
    addRow(tr("Visible (filtered)"),
           QString("%1  (%2%)").arg(visibleEvents).arg(visPct, 0, 'f', 1));

    addRow(tr("Unique type values"),
           uniqueTypes.empty() ? tr("N/A") : QString::number(uniqueTypes.size()));

    if (tMin.isValid())
    {
        constexpr char kFmt[] = "yyyy-MM-dd HH:mm:ss";
        addRow(tr("First event"), tMin.toString(QLatin1String(kFmt)));
        addRow(tr("Last event"),  tMax.toString(QLatin1String(kFmt)));

        const qint64 spanSecs = tMin.secsTo(tMax);
        if (spanSecs >= 0)
        {
            const int hours = static_cast<int>(spanSecs / 3600);
            const int mins  = static_cast<int>((spanSecs % 3600) / 60);
            const int secs  = static_cast<int>(spanSecs % 60);
            addRow(tr("Duration"),
                   QString("%1h %2m %3s")
                       .arg(hours)
                       .arg(mins, 2, 10, QChar('0'))
                       .arg(secs, 2, 10, QChar('0')));

            const double rate = spanSecs > 0
                ? static_cast<double>(visibleEvents) * 60.0 / static_cast<double>(spanSecs)
                : 0.0;
            addRow(tr("Avg. rate"),
                   QString("%1 events/min").arg(rate, 0, 'f', 1));
        }
    }
    else
    {
        addRow(tr("Time range"), tr("N/A — no parseable timestamp field"));
    }

    m_summaryTable->resizeRowsToContents();
}

// ---------------------------------------------------------------------------
// Type chart
// ---------------------------------------------------------------------------

void StatsSummaryPanel::RefreshTypeChart(const std::vector<unsigned long>& indices)
{
    const std::string& typeField = config::GetConfig().typeFilterField;
    const size_t       total     = indices.size();

    std::map<std::string, int> counts;
    for (unsigned long idx : indices)
    {
        const std::string val =
            m_events.GetEvent(static_cast<int>(idx)).findByKey(typeField);
        counts[val.empty() ? "(none)" : val]++;
    }

    using Pair = std::pair<std::string, int>;
    std::vector<Pair> sorted(counts.begin(), counts.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const Pair& a, const Pair& b) { return a.second > b.second; });
    if (sorted.size() > 15)
        sorted.resize(15);

    auto* barSet = new QBarSet(QString::fromStdString(typeField));
    QStringList categories;
    QStringList tooltips; // full-text tooltips for hover

    for (const auto& [label, count] : sorted)
    {
        *barSet << count;
        const QString qLabel = QString::fromStdString(label);

        // Truncate labels that are too long for the axis tick
        constexpr int kMaxLabelLen = 22;
        categories << (qLabel.length() > kMaxLabelLen
                           ? qLabel.left(kMaxLabelLen - 1) + QChar(u'\u2026') // …
                           : qLabel);

        const double pct =
            total > 0 ? 100.0 * count / static_cast<double>(total) : 0.0;
        tooltips << QString("%1\nCount: %2  (%3%)")
                        .arg(qLabel)
                        .arg(count)
                        .arg(pct, 0, 'f', 1);
    }

    // Hover tooltip — QHorizontalBarSeries draws categories bottom→top
    // so bar index 0 = bottom bar = sorted[0].
    connect(barSet, &QBarSet::hovered, this,
            [tooltips](bool status, int index) {
                if (status && index >= 0 && index < tooltips.size())
                    QToolTip::showText(QCursor::pos(), tooltips.at(index));
                else
                    QToolTip::hideText();
            });

    auto* series = new QHorizontalBarSeries();
    series->append(barSet);
    series->setLabelsVisible(true);
    series->setLabelsPosition(QAbstractBarSeries::LabelsOutsideEnd);
    series->setLabelsFormat("@value");

    const int nBars = static_cast<int>(sorted.size());
    auto* chart = new QChart();
    chart->addSeries(series);
    chart->setTitle(tr("Event Types (top %1)").arg(nBars));
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

    const int chartH = std::max(150, nBars * 28 + 60);
    m_typeChartView->setMinimumHeight(chartH);
    m_typeChartView->setChart(chart);
}

// ---------------------------------------------------------------------------
// Top-N table
// ---------------------------------------------------------------------------

void StatsSummaryPanel::RefreshTopNTable(const std::vector<unsigned long>& indices)
{
    auto* model = m_eventsView ? m_eventsView->model() : nullptr;
    if (!model || m_columnCombo->count() == 0)
    {
        m_topNTable->setRowCount(0);
        return;
    }

    const int    col   = m_columnCombo->currentData().toInt();
    const int    topN  = m_topNSpin->value();
    const size_t total = indices.size();

    std::map<QString, int> counts;
    for (unsigned long idx : indices)
    {
        const QString val = model->data(
            model->index(static_cast<int>(idx), col), Qt::DisplayRole).toString();
        counts[val]++;
    }

    using Pair = std::pair<QString, int>;
    std::vector<Pair> sorted(counts.begin(), counts.end());
    std::sort(sorted.begin(), sorted.end(),
              [](const Pair& a, const Pair& b) { return a.second > b.second; });
    if (static_cast<int>(sorted.size()) > topN)
        sorted.resize(static_cast<size_t>(topN));

    m_topNTable->setRowCount(static_cast<int>(sorted.size()));

    for (int r = 0; r < static_cast<int>(sorted.size()); ++r)
    {
        const auto& [value, count] = sorted[static_cast<size_t>(r)];
        const double pct =
            total > 0 ? 100.0 * count / static_cast<double>(total) : 0.0;

        auto* rankItem = new QTableWidgetItem(QString::number(r + 1));
        auto* valItem  = new QTableWidgetItem(value);
        auto* cntItem  = new QTableWidgetItem(QString::number(count));
        auto* pctItem  = new QTableWidgetItem(
            QString("%1%").arg(pct, 0, 'f', 1));

        // Tooltip shows full text when the value column is too narrow to display it
        valItem->setToolTip(value);

        rankItem->setTextAlignment(Qt::AlignCenter);
        cntItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
        pctItem->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);

        m_topNTable->setItem(r, 0, rankItem);
        m_topNTable->setItem(r, 1, valItem);
        m_topNTable->setItem(r, 2, cntItem);
        m_topNTable->setItem(r, 3, pctItem);
    }
}

// ---------------------------------------------------------------------------
// Field statistics
// ---------------------------------------------------------------------------

void StatsSummaryPanel::RefreshFieldStats(const std::vector<unsigned long>& indices)
{
    auto* model = m_eventsView ? m_eventsView->model() : nullptr;
    if (!model)
    {
        m_fieldStatsTable->setRowCount(0);
        return;
    }

    const int    cols  = model->columnCount();
    const size_t total = indices.size();

    // For very large datasets: stride-sample to avoid O(n×cols) UI freeze.
    // A sample of 50 000 rows gives < 1 % statistical error on fill rate.
    constexpr size_t kFieldStatsSample    = 50'000;
    constexpr size_t kUniqueTrackingLimit = 50'000;
    constexpr size_t kMaxUniquesPerField  = 1'000;
    const size_t step        = total > kFieldStatsSample ? total / kFieldStatsSample : 1;
    const size_t sampleTotal = (total + step - 1) / step;
    const bool   sampled     = step > 1;
    const bool   trackUniques = (sampleTotal <= kUniqueTrackingLimit);

    m_fieldStatsTable->setRowCount(cols);

    for (int c = 0; c < cols; ++c)
    {
        const QString colName =
            model->headerData(c, Qt::Horizontal, Qt::DisplayRole).toString();

        std::set<QString> uniqueVals;
        int filled = 0;

        for (size_t si = 0; si < total; si += step)
        {
            const QString val = model->data(
                model->index(static_cast<int>(indices[si]), c),
                Qt::DisplayRole).toString();
            if (!val.isEmpty())
            {
                ++filled;
                if (trackUniques && uniqueVals.size() < kMaxUniquesPerField)
                    uniqueVals.insert(val);
            }
        }

        const double fillPct =
            sampleTotal > 0 ? 100.0 * filled / static_cast<double>(sampleTotal) : 0.0;

        const QString uniqueStr = !trackUniques
            ? tr(">50k")
            : (uniqueVals.size() >= kMaxUniquesPerField
               ? tr(">%1").arg(kMaxUniquesPerField)
               : QString::number(uniqueVals.size()));

        auto* nameItem   = new QTableWidgetItem(colName);
        auto* uniqueItem = new QTableWidgetItem(uniqueStr);
        auto* fillItem   = new QTableWidgetItem(
            sampled ? QString("%1%~").arg(fillPct, 0, 'f', 1)
                    : QString("%1%").arg(fillPct, 0, 'f', 1));

        nameItem->setToolTip(colName); // full name if column is narrow
        if (sampled) fillItem->setToolTip(tr("Approximate — computed from a %1-row sample").arg(sampleTotal));
        uniqueItem->setTextAlignment(Qt::AlignCenter);
        fillItem->setTextAlignment(Qt::AlignCenter);

        // Colour-code fill rate: red < 50 %, amber < 90 %, otherwise default
        if (fillPct < 50.0)
            fillItem->setForeground(QBrush(Qt::red));
        else if (fillPct < 90.0)
            fillItem->setForeground(QBrush(QColor(200, 120, 0)));

        m_fieldStatsTable->setItem(c, 0, nameItem);
        m_fieldStatsTable->setItem(c, 1, uniqueItem);
        m_fieldStatsTable->setItem(c, 2, fillItem);
    }

    m_fieldStatsTable->resizeRowsToContents();
}

} // namespace ui::qt

