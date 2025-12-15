#pragma once

#include "MetricsEngine.hpp"

#include <QObject>
#include <QWidget>
#include <vector>

class AnomalyChartWidget : public QWidget
{
    Q_OBJECT
public:
    explicit AnomalyChartWidget(QWidget* parent = nullptr);

    void SetResults(const std::vector<MetricsEngine::Result>& results);
    void SetHighlightIndex(int index);
    void SetRuleNames(const std::vector<std::string>& names);

protected:
    void paintEvent(QPaintEvent* event) override;
    QSize minimumSizeHint() const override { return {240, 160}; }

private:
    std::vector<MetricsEngine::Result> m_results;
    int m_highlightIndex = -1;
    std::vector<std::string> m_ruleNames;
};
