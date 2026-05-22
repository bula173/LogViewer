#pragma once

#include "IView.hpp"

#include <QChartView>
#include <QLabel>
#include <QWidget>

#include <string>
#include <vector>

namespace db {
class EventsContainer;
}

namespace ui::qt {

/**
 * @brief Signal value plot panel.
 *
 * Plots the numeric/signal fields selected in the Signal Browser dock.
 * Call SetSelectedSignals() whenever the selection changes; the chart
 * updates immediately.  Refresh() is also called lazily by MainWindow
 * when the tab first becomes visible.
 */
class SignalPlotPanel : public QWidget, public mvc::IView
{
    Q_OBJECT

  public:
    explicit SignalPlotPanel(db::EventsContainer& events, QWidget* parent = nullptr);

    /// Called by MainWindow when the Signals tab becomes active.
    void Refresh();

    /// Update the set of signals to plot and redraw the chart immediately.
    Q_SLOT void SetSelectedSignals(const std::vector<std::string>& keys);

    // mvc::IView ---------------------------------------------------------------
    void OnDataUpdated() override;
    void OnCurrentIndexUpdated(int) override {}

  private:
    void BuildLayout();
    void RebuildChart();

    static double ToSeconds(const std::string& raw, double firstSec);

    db::EventsContainer& m_events;

    QChartView* m_chartView   {nullptr};
    QLabel*     m_statusLabel {nullptr};

    std::vector<std::string> m_selectedSignals;
    bool m_dirty {false};
};

} // namespace ui::qt
