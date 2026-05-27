#include "TimelineChartPanel.hpp"

#include "Config.hpp"
#include "utils/PanelUtils.hpp"

#include <QAbstractBarSeries>
#include <QBarCategoryAxis>
#include <QBarSet>
#include <QChart>
#include <QChartView>
#include <QColor>
#include <QDateTime>
#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>
#include <QSpinBox>
#include <QStackedBarSeries>
#include <QValueAxis>
#include <QVBoxLayout>

#include <algorithm>
#include <array>
#include <cctype>
#include <map>
#include <set>
#include <string>

namespace ui::qt {

TimelineChartPanel::TimelineChartPanel(db::EventsContainer& events,
                                       EventsTableView*     eventsView,
                                       QWidget*             parent)
    : QWidget(parent), m_events(events), m_eventsView(eventsView)
{
    BuildLayout();
}

void TimelineChartPanel::BuildLayout()
{
    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(6, 6, 6, 6);
    layout->setSpacing(6);

    // ── Controls row ──────────────────────────────────────────────────────
    auto* ctrlRow = new QHBoxLayout();
    ctrlRow->addWidget(new QLabel(tr("Buckets:"), this));
    m_bucketSpin = new QSpinBox(this);
    m_bucketSpin->setRange(5, 50);
    m_bucketSpin->setValue(20);
    m_bucketSpin->setToolTip(tr("Number of time buckets in the histogram (5–50)"));
    ctrlRow->addWidget(m_bucketSpin);
    ctrlRow->addStretch();
    m_clearBtn = new QPushButton(tr("Clear Selection"), this);
    m_clearBtn->setToolTip(tr("Remove the time-bucket filter and show all visible events"));
    m_clearBtn->setEnabled(false);
    ctrlRow->addWidget(m_clearBtn);
    layout->addLayout(ctrlRow);

    // ── Chart ─────────────────────────────────────────────────────────────
    m_chartView = new QChartView(this);
    m_chartView->setRenderHint(QPainter::Antialiasing);
    m_chartView->setMinimumHeight(250);
    m_chartView->setChart(new QChart());
    layout->addWidget(m_chartView, 1);

    // ── Status label ──────────────────────────────────────────────────────
    m_statusLabel = new QLabel(tr("No data"), this);
    m_statusLabel->setAlignment(Qt::AlignRight);
    layout->addWidget(m_statusLabel);

    // ── Connections ───────────────────────────────────────────────────────
    connect(m_bucketSpin, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &TimelineChartPanel::Refresh);

    connect(m_clearBtn, &QPushButton::clicked, this, [this]() {
        m_eventsView->ClearFilter();
        m_clearBtn->setEnabled(false);
    });
}

// ---------------------------------------------------------------------------
// Refresh
// ---------------------------------------------------------------------------

void TimelineChartPanel::Refresh()
{
    auto* chart = new QChart();
    chart->setAnimationOptions(QChart::NoAnimation);
    chart->setMargins(QMargins(4, 4, 4, 4));
    m_bucketEvents.clear();

    const auto vis = panel_utils::VisibleIndices(m_eventsView, m_events);

    if (vis.empty())
    {
        chart->setTitle(tr("Events Over Time"));
        m_chartView->setChart(chart);
        m_statusLabel->setText(tr("No events"));
        m_clearBtn->setEnabled(false);
        return;
    }

    const std::string tsField = DetectTimestampField(m_events, vis);
    if (tsField.empty())
    {
        chart->setTitle(tr("Events Over Time (no timestamp field found)"));
        m_chartView->setChart(chart);
        m_statusLabel->setText(tr("No timestamp field detected"));
        m_clearBtn->setEnabled(false);
        return;
    }

    // ── Collect (time, index) pairs ───────────────────────────────────────
    std::vector<std::pair<QDateTime, unsigned long>> timed;
    timed.reserve(vis.size());
    for (unsigned long idx : vis)
    {
        const QDateTime dt = panel_utils::ParseTimestamp(QString::fromStdString(
            m_events.GetEvent(idx).findByKey(tsField)));
        if (dt.isValid())
            timed.emplace_back(dt, idx);
    }

    if (timed.size() < 2)
    {
        chart->setTitle(tr("Events Over Time (insufficient timestamp data)"));
        m_chartView->setChart(chart);
        m_statusLabel->setText(tr("Too few timestamped events"));
        m_clearBtn->setEnabled(false);
        return;
    }

    std::sort(timed.begin(), timed.end(),
              [](const auto& a, const auto& b) { return a.first < b.first; });

    const QDateTime tMin     = timed.front().first;
    const QDateTime tMax     = timed.back().first;
    const qint64    spanMs   = tMin.msecsTo(tMax);
    const int       buckets  = m_bucketSpin->value();
    const qint64    bucketMs = std::max(qint64(1), spanMs / buckets);
    const QString   labelFmt = (spanMs > 86'400'000LL) ? "MM-dd HH:mm" : "HH:mm:ss";

    // ── Detect level field and collect unique levels ──────────────────────
    const std::string levelField = config::GetConfig().typeFilterField;

    std::set<std::string> levelSet;
    for (const auto& [dt, idx] : timed)
    {
        const std::string lv =
            m_events.GetEvent(idx).findByKey(levelField);
        if (!lv.empty())
            levelSet.insert(lv);
    }
    if (levelSet.empty())
        levelSet.insert("events");

    // ── Assign colours ────────────────────────────────────────────────────
    static const std::map<std::string, QColor> kKnownColors{
        {"error",    {220,  50,  47}},
        {"critical", {220,  50,  47}},
        {"fatal",    {196,  30,  58}},
        {"warn",     {181, 137,   0}},
        {"warning",  {181, 137,   0}},
        {"info",     { 38, 139, 210}},
        {"notice",   { 42, 161, 152}},
        {"debug",    {108, 113, 196}},
        {"trace",    {133, 153, 153}},
        {"events",   { 38, 139, 210}},
    };
    static const std::array<QColor, 5> kFallbacks{
        QColor{38, 139, 210}, QColor{42, 161, 152}, QColor{108, 113, 196},
        QColor{181, 137, 0},  QColor{220,  50,  47},
    };

    std::map<std::string, QColor> levelColors;
    int fallbackIdx = 0;
    for (const auto& lv : levelSet)
    {
        std::string lower = lv;
        std::transform(lower.begin(), lower.end(), lower.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        const auto it = kKnownColors.find(lower);
        levelColors[lv] = (it != kKnownColors.end())
            ? it->second
            : kFallbacks[static_cast<size_t>(fallbackIdx++) % kFallbacks.size()];
    }

    // ── Build bucket data ─────────────────────────────────────────────────
    m_bucketEvents.assign(static_cast<size_t>(buckets), std::vector<unsigned long>{});

    // [level][bucket] = count
    std::map<std::string, std::vector<int>> lvBucketCounts;
    for (const auto& lv : levelSet)
        lvBucketCounts[lv].assign(static_cast<size_t>(buckets), 0);

    for (const auto& [dt, idx] : timed)
    {
        qint64 offset = tMin.msecsTo(dt);
        int b = static_cast<int>(offset / bucketMs);
        if (b >= buckets) b = buckets - 1;
        m_bucketEvents[static_cast<size_t>(b)].push_back(idx);

        const std::string lv =
            m_events.GetEvent(idx).findByKey(levelField);
        const std::string key = levelSet.count(lv) ? lv : *levelSet.begin();
        lvBucketCounts[key][static_cast<size_t>(b)]++;
    }

    // ── Build QBarSets and series ─────────────────────────────────────────
    QStringList categories;
    for (int i = 0; i < buckets; ++i)
        categories << tMin.addMSecs(i * bucketMs).toString(labelFmt);

    auto* series = new QStackedBarSeries();
    for (const auto& lv : levelSet)
    {
        auto* barSet = new QBarSet(QString::fromStdString(lv));
        barSet->setColor(levelColors[lv]);
        for (int i = 0; i < buckets; ++i)
            *barSet << lvBucketCounts[lv][static_cast<size_t>(i)];
        series->append(barSet);
    }

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

    // Click → filter to events in that bucket
    connect(series, &QAbstractBarSeries::clicked,
            this, [this](int index, QBarSet*) {
                const int sz = static_cast<int>(m_bucketEvents.size());
                if (index >= 0 && index < sz &&
                    !m_bucketEvents[static_cast<size_t>(index)].empty())
                {
                    m_eventsView->SetFilteredEvents(
                        m_bucketEvents[static_cast<size_t>(index)]);
                    m_clearBtn->setEnabled(true);
                }
            });

    m_chartView->setChart(chart);

    m_statusLabel->setText(
        tr("%1 events, %2 to %3")
            .arg(timed.size())
            .arg(tMin.toString("yyyy-MM-dd HH:mm:ss"))
            .arg(tMax.toString("yyyy-MM-dd HH:mm:ss")));
    m_clearBtn->setEnabled(false);
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

std::string TimelineChartPanel::DetectTimestampField(
    db::EventsContainer&              events,
    const std::vector<unsigned long>& indices)
{
    for (const auto& c : panel_utils::kTsFields)
        for (unsigned long idx : indices)
        {
            const std::string val =
                events.GetEvent(idx).findByKey(c);
            if (!val.empty() &&
                panel_utils::ParseTimestamp(QString::fromStdString(val)).isValid())
                return c;
        }
    return {};
}

} // namespace ui::qt
