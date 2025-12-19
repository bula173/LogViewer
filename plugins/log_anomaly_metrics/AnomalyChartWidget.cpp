#include "AnomalyChartWidget.hpp"

#include <QPainter>
#include <QPaintEvent>
#include <QPainterPath>
#include <algorithm>
#include <cmath>
#include <limits>
#include <QDateTime>
#include "moc_AnomalyChartWidget.cpp"

AnomalyChartWidget::AnomalyChartWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumHeight(180);
}

void AnomalyChartWidget::SetResults(const std::vector<MetricsEngine::Result>& results)
{
    m_results = results;
    update();
}

void AnomalyChartWidget::SetHighlightIndex(int index)
{
    m_highlightIndex = index;
    update();
}

void AnomalyChartWidget::SetRuleNames(const std::vector<std::string>& names)
{
    m_ruleNames = names;
    update();
}

void AnomalyChartWidget::paintEvent(QPaintEvent* event)
{
    QWidget::paintEvent(event);

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);

    const int left = 40;
    const int right = 10;
    const int top = 10;
    const int bottom = 30;

    const QRect plotRect(left, top, width() - left - right, height() - top - bottom);
    p.fillRect(rect(), palette().window());
    p.setPen(palette().dark().color());
    p.drawRect(plotRect.adjusted(0, 0, -1, -1));

    if (m_results.empty() || plotRect.width() <= 0 || plotRect.height() <= 0)
    {
        p.drawText(rect(), Qt::AlignCenter, tr("No data"));
        return;
    }

    double maxScore = 0.0;
    for (const auto& r : m_results)
    {
        for (double v : r.ruleScores)
        {
            maxScore = std::max(maxScore, std::abs(v));
        }
    }
    if (maxScore <= 0.0)
    {
        maxScore = 1.0;
    }

    const int n = static_cast<int>(m_results.size());
    const double xStep = n > 1 ? static_cast<double>(plotRect.width()) / static_cast<double>(n - 1) : 0.0;

    // Build X coordinates from timestamps when available
    std::vector<double> xs(static_cast<size_t>(n), 0.0);
    auto parseTsMs = [](const std::string& ts) -> double {
        if (ts.empty()) return std::numeric_limits<double>::quiet_NaN();
        const auto qts = QString::fromStdString(ts);
        QDateTime dt = QDateTime::fromString(qts, Qt::ISODateWithMs);
        if (!dt.isValid()) dt = QDateTime::fromString(qts, Qt::ISODate);
        if (!dt.isValid())
        {
            bool ok = false;
            qint64 ms = qts.toLongLong(&ok);
            if (ok) return static_cast<double>(ms);
            return std::numeric_limits<double>::quiet_NaN();
        }
        return static_cast<double>(dt.toMSecsSinceEpoch());
    };

    bool hasValidTime = false;
    for (int i = 0; i < n; ++i)
    {
        const double t = parseTsMs(m_results[static_cast<size_t>(i)].timestamp);
        xs[static_cast<size_t>(i)] = t;
        if (!std::isnan(t)) hasValidTime = true;
    }
    if (!hasValidTime)
    {
        for (int i = 0; i < n; ++i) xs[static_cast<size_t>(i)] = static_cast<double>(i);
    }

    double minX = *std::min_element(xs.begin(), xs.end());
    double maxX = *std::max_element(xs.begin(), xs.end());
    if (std::isnan(minX) || std::isnan(maxX) || minX == maxX)
    {
        minX = 0.0;
        maxX = std::max(1.0, static_cast<double>(n - 1));
    }

    auto xToPx = [&](double x) {
        return plotRect.left() + (x - minX) * (static_cast<double>(plotRect.width()) / (maxX - minX));
    };

    // Color palette per rule
    const int ruleCount = static_cast<int>(m_ruleNames.size());
    auto colorForRule = [&](int idx) {
        return QColor::fromHsv((idx * 53) % 360, 200, 230);
    };

    // Draw each rule as a separate polyline
    for (int r = 0; r < ruleCount; ++r)
    {
        QPainterPath path;
        bool started = false;
        for (int i = 0; i < n; ++i)
        {
            if (r >= static_cast<int>(m_results[static_cast<size_t>(i)].ruleScores.size()))
                continue;
            const double val = m_results[static_cast<size_t>(i)].ruleScores[static_cast<size_t>(r)];
            const double normY = val / maxScore;
            const double x = xToPx(xs[static_cast<size_t>(i)]);
            const double y = plotRect.bottom() - (normY * plotRect.height());
            if (!started)
            {
                path.moveTo(QPointF(x, y));
                started = true;
            }
            else
            {
                path.lineTo(QPointF(x, y));
            }
        }
        p.setPen(QPen(colorForRule(r), 2));
        p.drawPath(path);
    }

    // X axis ticks every ~max(1, n/10)
    const int tickEvery = std::max(1, n / 10);
    p.setPen(palette().mid().color());
    for (int i = 0; i < n; i += tickEvery)
    {
        const double x = plotRect.left() + i * xStep;
        p.drawLine(QPointF(x, plotRect.bottom()), QPointF(x, plotRect.bottom() + 4));
        p.drawText(QPointF(x - 10, plotRect.bottom() + 16), QString::number(m_results[static_cast<size_t>(i)].index));
    }

    // Highlight selected index
    if (m_highlightIndex >= 0)
    {
        // find nearest point with matching index
        for (int i = 0; i < n; ++i)
        {
            if (static_cast<int>(m_results[static_cast<size_t>(i)].index) == m_highlightIndex)
            {
                const double val = m_results[static_cast<size_t>(i)].score;
                const double normY = val / maxScore;
                const double x = xToPx(xs[static_cast<size_t>(i)]);
                const double y = plotRect.bottom() - (normY * plotRect.height());
                p.setPen(QPen(Qt::red, 1, Qt::DashLine));
                p.drawLine(QPointF(x, plotRect.top()), QPointF(x, plotRect.bottom()));
                p.setBrush(Qt::red);
                p.setPen(Qt::NoPen);
                p.drawEllipse(QPointF(x, y), 4.0, 4.0);
                break;
            }
        }
    }

    // Y axis label max
    p.setPen(palette().text().color());
    p.drawText(QRect(0, plotRect.top(), left - 4, plotRect.height()), Qt::AlignRight | Qt::AlignTop,
        QString::number(maxScore, 'f', 2));
    p.drawText(QRect(0, plotRect.top(), left - 4, plotRect.height()), Qt::AlignRight | Qt::AlignBottom,
        QString::number(-maxScore, 'f', 2));
}
