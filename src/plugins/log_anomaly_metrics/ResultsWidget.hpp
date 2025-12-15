#pragma once

#include "MetricsEngine.hpp"

#include <QTableWidget>
#include <QWidget>
#include <vector>

class ResultsWidget : public QWidget
{
    Q_OBJECT
public:
    explicit ResultsWidget(QWidget* parent = nullptr);
    void DisplayResults(const std::vector<MetricsEngine::Result>& results);

signals:
    void EventActivated(int index);

private:
    QTableWidget* m_table = nullptr;
};
