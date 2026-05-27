#include "SignalPlotPanel.hpp"

#include "EventsContainer.hpp"
#include "utils/PanelUtils.hpp"

#include <QChart>
#include <QChartView>
#include <QLabel>
#include <QLineSeries>
#include <QPainter>
#include <QValueAxis>
#include <QVBoxLayout>

#include <array>
#include <cmath>
#include <limits>
#include <string>

namespace ui::qt {

static constexpr int kMaxPoints = 2000;

static const std::array<QColor, 8> kPalette{{
    {  38, 139, 210},
    { 220,  50,  47},
    {  42, 161, 152},
    { 181, 137,   0},
    { 108, 113, 196},
    { 133, 153, 153},
    { 211,  54, 130},
    { 101, 123, 131},
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
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(2);

    m_chartView = new QChartView(this);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setMinimumHeight(200);
    m_chartView->setChart(new QChart());
    layout->addWidget(m_chartView, 1);

    m_statusLabel = new QLabel(tr("Select signals in the Signal Browser panel"), this);
    m_statusLabel->setAlignment(Qt::AlignRight);
    layout->addWidget(m_statusLabel);
}

// ---------------------------------------------------------------------------
// IView
// ---------------------------------------------------------------------------

void SignalPlotPanel::OnDataUpdated()
{
    m_dirty = true;
}

// ---------------------------------------------------------------------------
// Public
// ---------------------------------------------------------------------------

void SignalPlotPanel::Refresh()
{
    if (m_dirty)
    {
        RebuildChart();
        m_dirty = false;
    }
}

void SignalPlotPanel::SetSelectedSignals(const std::vector<std::string>& keys)
{
    m_selectedSignals = keys;
    RebuildChart();
}

// ---------------------------------------------------------------------------
// Private helpers
// ---------------------------------------------------------------------------

double SignalPlotPanel::ToSeconds(const std::string& raw, double firstSec)
{
    if (raw.empty())
        return std::numeric_limits<double>::quiet_NaN();

    const QString qs = QString::fromStdString(raw);
    bool ok = false;
    const double d = qs.toDouble(&ok);
    if (ok)
        return d - firstSec;

    const QDateTime dt = panel_utils::ParseTimestamp(qs);
    if (dt.isValid())
        return static_cast<double>(dt.toMSecsSinceEpoch()) / 1000.0 - firstSec;

    return std::numeric_limits<double>::quiet_NaN();
}

void SignalPlotPanel::RebuildChart()
{
    auto* chart = new QChart();
    chart->setAnimationOptions(QChart::NoAnimation);
    chart->setMargins(QMargins(4, 4, 4, 4));
    chart->legend()->setVisible(true);
    chart->legend()->setAlignment(Qt::AlignBottom);

    if (m_selectedSignals.empty() || m_events.Size() == 0)
    {
        chart->setTitle(tr("Signal Plot — select signals in the Signal Browser"));
        m_chartView->setChart(chart);
        m_statusLabel->setText(m_selectedSignals.empty()
            ? tr("No signals selected — use the Signal Browser panel")
            : tr("No data loaded"));
        return;
    }

    // Detect timestamp field.
    const size_t total = m_events.Size();
    std::string tsField;
    for (const auto& candidate : panel_utils::kTsFields)
    {
        for (size_t i = 0; i < std::min(total, size_t{20}); ++i)
        {
            if (!m_events.GetItem(i).findByKey(candidate).empty())
            {
                tsField = candidate;
                break;
            }
        }
        if (!tsField.empty()) break;
    }

    // Time origin for relative-seconds axis.
    double firstSec = 0.0;
    if (!tsField.empty())
    {
        const QString qs = QString::fromStdString(
            m_events.GetItem(0).findByKey(tsField));
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

    const size_t step = (total > static_cast<size_t>(kMaxPoints))
                      ? (total / static_cast<size_t>(kMaxPoints)) : 1u;

    double xMin = std::numeric_limits<double>::max();
    double xMax = std::numeric_limits<double>::lowest();
    double yMin = std::numeric_limits<double>::max();
    double yMax = std::numeric_limits<double>::lowest();

    std::vector<QLineSeries*> seriesList;
    int colourIdx = 0;

    for (const auto& key : m_selectedSignals)
    {
        auto* series = new QLineSeries();
        series->setName(QString::fromStdString(key.substr(4))); // strip "SIG:"
        series->setColor(kPalette[static_cast<size_t>(colourIdx++) % kPalette.size()]);

        for (size_t i = 0; i < total; i += step)
        {
            const auto& ev = m_events.GetItem(i);

            double x = static_cast<double>(i);
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
        chart->setTitle(tr("Signal Plot — no numeric values found for selected signals"));
        m_chartView->setChart(chart);
        m_statusLabel->setText(tr("No plottable values"));
        return;
    }

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
