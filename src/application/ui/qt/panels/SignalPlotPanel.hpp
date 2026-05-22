#pragma once

#include "IView.hpp"

#include <QChartView>
#include <QLabel>
#include <QWidget>

#include <string>
#include <vector>

class QListWidget;
class QSplitter;

namespace db {
class EventsContainer;
}

namespace ui::qt {

/**
 * @brief Signal value plot panel.
 *
 * Shows how selected numeric/signal fields change over time.
 * The left pane lists all SIG:* fields discovered in the loaded events;
 * the user checks/unchecks signals to add or remove their line series from
 * the chart on the right.  Any numeric field (not just SIG:*) can also be
 * plotted by pressing the "Show all fields" toggle.
 *
 * Refresh() is called lazily by MainWindow when the tab becomes visible.
 */
class SignalPlotPanel : public QWidget, public mvc::IView
{
    Q_OBJECT

  public:
    explicit SignalPlotPanel(db::EventsContainer& events, QWidget* parent = nullptr);

    /// Rebuild chart from currently checked signals.  Called by MainWindow
    /// when the tab becomes active (lazy refresh pattern).
    void Refresh();

    // mvc::IView ---------------------------------------------------------------
    void OnDataUpdated() override;
    void OnCurrentIndexUpdated(int) override {}

  private slots:
    void OnSignalToggled(int row);

  private:
    void BuildLayout();

    /// Re-scan all events and repopulate the signal list, preserving checked state.
    void RebuildSignalList();

    /// Rebuild the QChart from the currently checked signals.
    void RebuildChart();

    /// Return elapsed seconds for a raw timestamp string.
    /// For float strings (ASC "0.001000") returns the double directly.
    /// For ISO/epoch strings returns seconds since the first event's timestamp.
    /// Returns quiet_NaN on failure.
    static double ToSeconds(const std::string& raw, double firstSec);

    db::EventsContainer& m_events;

    QListWidget*  m_signalList  {nullptr};
    QChartView*   m_chartView   {nullptr};
    QLabel*       m_statusLabel {nullptr};

    /// Keys currently shown in the list (full key, e.g. "SIG:RPM").
    std::vector<std::string> m_listedKeys;

    /// Set to true when data changes; triggers RebuildSignalList on next Refresh().
    bool m_needsListRebuild {true};
};

} // namespace ui::qt
