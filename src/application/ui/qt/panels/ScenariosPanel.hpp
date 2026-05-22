#pragma once

#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QTableWidget>
#include <QWidget>

#include <nlohmann/json.hpp>

#include <string>
#include <vector>

namespace db {
class EventsContainer;
}

namespace ui::qt {
class EventsTableView;
}

namespace ui::qt {

/**
 * @brief Scenario builder panel.
 *
 * A scenario is a named, ordered collection of log events assembled by the
 * analyst.  Events can be added from the Events table (via button or
 * right-click context menu), reordered, and removed.  The whole scenario
 * can be exported as plain text, a Markdown table, or JSON Lines — with
 * a field selector dialog to control which event items appear in the output.
 *
 * Multiple independent scenarios are supported via the combo box selector.
 * Scenarios persist in QSettings across sessions.
 */
class ScenariosPanel : public QWidget
{
    Q_OBJECT

  public:
    explicit ScenariosPanel(db::EventsContainer& events,
                            EventsTableView*     eventsView,
                            QWidget*             parent = nullptr);

  public slots:
    /// Add the event at @p actualRow to the currently active scenario.
    /// If no scenario exists yet a default one is created automatically.
    void AddEventFromRow(int actualRow);

  public:
    nlohmann::json GetSessionData() const;
    void LoadSessionData(const nlohmann::json& data);

  private slots:
    void OnNewScenario();
    void OnRenameScenario();
    void OnDeleteScenario();
    void OnScenarioChanged(int comboIndex);
    void OnAddSelectedEvent();
    void OnRemoveEvent();
    void OnMoveUp();
    void OnMoveDown();
    void OnExport();

  private:
    struct ScenarioEvent
    {
        int         row      {-1};
        std::string timestamp;
        std::string summary;
    };

    struct Scenario
    {
        std::string              name;
        std::vector<ScenarioEvent> events;
    };

    void BuildLayout();
    void RebuildEventsTable();
    void UpdateScenarioCombo(int selectIndex = -1);
    void UpdateButtonStates();

    /// Returns the index of the selected row in m_eventsTable, or -1.
    [[nodiscard]] int SelectedEventRow() const;

    /// Returns a pointer to the active Scenario, or nullptr if none.
    [[nodiscard]] Scenario* ActiveScenario();

    /// Build event-item summary strings for @p actualRow (cached on add).
    void FillEventStrings(ScenarioEvent& se) const;

    db::EventsContainer& m_events;
    EventsTableView*     m_eventsView;

    QComboBox*    m_scenarioCombo {nullptr};
    QPushButton*  m_newBtn        {nullptr};
    QPushButton*  m_renameBtn     {nullptr};
    QPushButton*  m_deleteBtn     {nullptr};
    QTableWidget* m_eventsTable   {nullptr};
    QPushButton*  m_addBtn        {nullptr};
    QPushButton*  m_removeBtn     {nullptr};
    QPushButton*  m_upBtn         {nullptr};
    QPushButton*  m_downBtn       {nullptr};
    QPushButton*  m_exportBtn     {nullptr};
    QLabel*       m_statusLabel   {nullptr};

    std::vector<Scenario> m_scenarios;
    int                   m_activeScenario {-1};
};

} // namespace ui::qt
