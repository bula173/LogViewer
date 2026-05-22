#include "SignalPlotPanel.hpp"

#include "EventsContainer.hpp"
#include "utils/PanelUtils.hpp"

#include <QChart>
#include <QChartView>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineSeries>
#include <QListWidget>
#include <QPainter>
#include <QSplitter>
#include <QValueAxis>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <set>
#include <string>

namespace ui::qt {

// Maximum data points per series — downsample beyond this to keep rendering fast.
static constexpr int kMaxPoints = 2000;

// Colour palette for series (cycles if more signals than colours).
static const std::array<QColor, 8> kPalette{{
    {  38, 139, 210},  // blue
    { 220,  50,  47},  // red
    {  42, 161, 152},  // teal
    { 181, 137,   0},  // yellow
    { 108, 113, 196},  // violet
    { 133, 153, 153},  // grey
    { 211,  54, 130},  // magenta
    { 101, 123, 131},  // slate
}};

// ---------------------------------------------------------------------------

SignalPlotPanel::SignalPlotPanel(db::EventsContainer& events, QWidget* parent)
    : QWidget(parent), m_events(events)
{
    BuildLayout();
    m_events.RegisterOndDataUpdated(this);
}

void SignalPlotPanel::BuildLayout()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(4, 4, 4, 4);
    outer->setSpacing(4);

    auto* splitter = new QSplitter(Qt::Horizontal, this);

    // ── Left: signal selector ────────────────────────────────────────────
    m_signalList = new QListWidget(splitter);
    m_signalList->setSelectionMode(QAbstractItemView::NoSelection);
    m_signalList->setToolTip(tr("Check signals to plot their values over time"));
    splitter->addWidget(m_signalList);

    // ── Right: chart ─────────────────────────────────────────────────────
    auto* rightWidget = new QWidget(splitter);
    auto* rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(2);

    m_chartView = new QChartView(rightWidget);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setMinimumHeight(200);
    m_chartView->setChart(new QChart());
    rightLayout->addWidget(m_chartView, 1);

    m_statusLabel = new QLabel(tr("No data"), rightWidget);
    m_statusLabel->setAlignment(Qt::AlignRight);
    rightLayout->addWidget(m_statusLabel);

    splitter->addWidget(rightWidget);
    splitter->setStretchFactor(0, 1);
    splitter->setStretchFactor(1, 4);

    outer->addWidget(splitter, 1);

    connect(m_signalList, &QListWidget::itemChanged,
            this, [this](QListWidgetItem* item) {
                const int row = m_signalList->row(item);
                if (row >= 0) OnSignalToggled(row);
            });
}

// ---------------------------------------------------------------------------
// IView
// ---------------------------------------------------------------------------

void SignalPlotPanel::OnDataUpdated()
{
    m_needsListRebuild = true;
}

// ---------------------------------------------------------------------------
// Public slot
// ---------------------------------------------------------------------------

void SignalPlotPanel::Refresh()
{
    if (m_needsListRebuild)
    {
        RebuildSignalList();
        m_needsListRebuild = false;
    }
    RebuildChart();
}

void SignalPlotPanel::OnSignalToggled(int /*row*/)
{
    RebuildChart();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

void SignalPlotPanel::RebuildSignalList()
{
    // Collect every SIG:* key across all events.
    std::set<std::string> found;
    const size_t total = m_events.Size();
    for (size_t i = 0; i < total; ++i)
    {
        for (const auto& [key, val] : m_events.GetItem(static_cast<int>(i)).getEventItems())
        {
            if (key.size() > 4 && key.substr(0, 4) == "SIG:")
                found.insert(key);
        }
    }

    // Remember which keys were checked so we can restore selection after repopulate.
    std::set<std::string> checked;
    for (int r = 0; r < m_signalList->count(); ++r)
    {
        auto* item = m_signalList->item(r);
        if (item->checkState() == Qt::Checked && r < static_cast<int>(m_listedKeys.size()))
            checked.insert(m_listedKeys[static_cast<size_t>(r)]);
    }

    // Block signals while repopulating to avoid per-item RebuildChart calls.
    QSignalBlocker blocker(m_signalList);
    m_signalList->clear();
    m_listedKeys.clear();
    m_listedKeys.reserve(found.size());

    for (const auto& key : found)
    {
        m_listedKeys.push_back(key);
        const QString display = QString::fromStdString(key.substr(4)); // strip "SIG:"
        auto* item = new QListWidgetItem(display, m_signalList);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(checked.count(key) ? Qt::Checked : Qt::Unchecked);
    }
}

// ---------------------------------------------------------------------------

double SignalPlotPanel::ToSeconds(const std::string& raw, double firstSec)
{
    if (raw.empty())
        return std::numeric_limits<double>::quiet_NaN();

    // Try plain float (ASC timestamps like "0.001000").
    const QString qs = QString::fromStdString(raw);
    bool ok = false;
    const double d = qs.toDouble(&ok);
    if (ok)
        return d - firstSec;

    // Try ISO/epoch via panel_utils.
    const QDateTime dt = panel_utils::ParseTimestamp(qs);
    if (dt.isValid())
        return static_cast<double>(dt.toMSecsSinceEpoch()) / 1000.0 - firstSec;

    return std::numeric_limits<double>::quiet_NaN();
}

// ---------------------------------------------------------------------------

void SignalPlotPanel::RebuildChart()
{
    auto* chart = new QChart();
    chart->setAnimationOptions(QChart::NoAnimation);
    chart->setMargins(QMargins(4, 4, 4, 4));
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    // Collect selected keys.
    std::vector<std::string> selected;
    for (int r = 0; r < m_signalList->count(); ++r)
    {
        auto* item = m_signalList->item(r);
        if (item && item->checkState() == Qt::Checked
                 && r < static_cast<int>(m_listedKeys.size()))
            selected.push_back(m_listedKeys[static_cast<size_t>(r)]);
    }

    if (selected.empty() || m_events.Size() == 0)
    {
        chart->setTitle(tr("Signal Plot — select signals on the left"));
        m_chartView->setChart(chart);
        m_statusLabel->setText(selected.empty() ? tr("No signals selected")
                                                : tr("No data loaded"));
        return;
    }

    // Detect timestamp field name.
    const size_t total = m_events.Size();
    std::string tsField;
    for (const auto& candidate : panel_utils::kTsFields)
    {
        for (size_t i = 0; i < std::min(total, size_t{20}); ++i)
        {
            const std::string val =
                m_events.GetItem(static_cast<int>(i)).findByKey(candidate);
            if (!val.empty()) { tsField = candidate; break; }
        }
        if (!tsField.empty()) break;
    }

    // Determine time origin (first event's timestamp) for relative-seconds axis.
    double firstSec = 0.0;
    if (!tsField.empty())
    {
        const std::string rawFirst =
            m_events.GetItem(0).findByKey(tsField);
        const QString qs = QString::fromStdString(rawFirst);
        bool ok = false;
        const double d = qs.toDouble(&ok);
        if (ok)
            firstSec = d;
        else
        {
            const QDateTime dt = panel_utils::ParseTimestamp(qs);
            if (dt.isValid())
                firstSec = static_cast<double>(dt.toMSecsSinceEpoch()) / 1000.0;
        }
    }

    // Build per-signal (time, value) point lists.
    // We downsample if total events > kMaxPoints.
    const size_t step = (total > static_cast<size_t>(kMaxPoints))
                      ? (total / static_cast<size_t>(kMaxPoints))
                      : 1u;

    double xMin = std::numeric_limits<double>::max();
    double xMax = std::numeric_limits<double>::lowest();
    double yMin = std::numeric_limits<double>::max();
    double yMax = std::numeric_limits<double>::lowest();

    std::vector<QLineSeries*> seriesList;
    int colourIdx = 0;

    for (const auto& key : selected)
    {
        auto* series = new QLineSeries();
        series->setName(QString::fromStdString(key.substr(4))); // strip SIG:
        series->setColor(kPalette[static_cast<size_t>(colourIdx++) % kPalette.size()]);

        for (size_t i = 0; i < total; i += step)
        {
            const auto& ev = m_events.GetItem(static_cast<int>(i));

            double x = static_cast<double>(i); // fallback: event index
            if (!tsField.empty())
            {
                const double t = ToSeconds(ev.findByKey(tsField), firstSec);
                if (!std::isnan(t)) x = t;
            }

            const std::string valStr = ev.findByKey(key);
            if (valStr.empty()) continue;

            bool ok = false;
            const double y = QString::fromStdString(valStr).toDouble(&ok);
            if (!ok) continue;

            series->append(x, y);
            xMin = std::min(xMin, x);
            xMax = std::max(xMax, x);
            yMin = std::min(yMin, y);
            yMax = std::max(yMax, y);
        }

        if (series->count() > 0)
        {
            chart->addSeries(series);
            seriesList.push_back(series);
        }
        else
        {
            delete series;
        }
    }

    if (seriesList.empty())
    {
        chart->setTitle(tr("Signal Plot — no numeric values found in selected signals"));
        m_chartView->setChart(chart);
        m_statusLabel->setText(tr("No plottable values"));
        return;
    }

    // Axes.
    const double xPad = (xMax > xMin) ? (xMax - xMin) * 0.02 : 1.0;
    const double yPad = (yMax > yMin) ? (yMax - yMin) * 0.05 : 1.0;

    auto* axisX = new QValueAxis();
    axisX->setRange(xMin - xPad, xMax + xPad);
    axisX->setTitleText(tsField.empty() ? tr("Event index") : tr("Time (s)"));
    axisX->setLabelFormat("%.3f");
    chart->addAxis(axisX, Qt::AlignBottom);

    auto* axisY = new QValueAxis();
    axisY->setRange(yMin - yPad, yMax + yPad);
    axisY->setLabelFormat("%.2f");
    chart->addAxis(axisY, Qt::AlignLeft);

    for (auto* s : seriesList)
    {
        s->attachAxis(axisX);
        s->attachAxis(axisY);
    }

    m_chartView->setChart(chart);

    const int pts = seriesList.empty() ? 0 : seriesList.front()->count();
    m_statusLabel->setText(
        tr("%1 signal(s), %2 point(s)%3")
            .arg(seriesList.size())
            .arg(pts)
            .arg(step > 1 ? tr(" (downsampled 1:%1)").arg(step) : QString{}));
}

} // namespace ui::qt
