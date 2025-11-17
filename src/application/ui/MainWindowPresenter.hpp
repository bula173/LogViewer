#pragma once

#include "db/EventsContainer.hpp"
#include "mvc/IControler.hpp"
#include "parser/IDataParser.hpp"
#include "ui/IEventsView.hpp"
#include "ui/IMainWindowView.hpp"
#include "ui/IUiPanels.hpp"

#include <filesystem>
#include <cstddef>

namespace ui
{

/**
 * @brief Pure C++ presenter that orchestrates main-window interactions
 * independently of the underlying widget toolkit.
 */
class MainWindowPresenter : public parser::IDataParserObserver
{
  public:
    MainWindowPresenter(IMainWindowView& view, mvc::IController& controller,
      db::EventsContainer& events, ui::ISearchResultsView& searchResults,
      ui::IEventsListView* eventsListView,
      ui::ITypeFilterView* typeFilterView,
      ui::IItemDetailsView* itemDetailsView);

    /** @brief Runs the search pipeline using the current UI state. */
    void PerformSearch();

    /** @brief Parses and loads a log file, updating shared UI state. */
    void LoadLogFile(const std::filesystem::path& path);

    /** @brief Allows views to query whether parsing is active. */
    bool IsParsing() const { return m_isParsing; }

    /** @brief Rebuilds the available type filters from current events. */
    void UpdateTypeFilters();

    /** @brief Applies the selected type filters to the event list. */
    void ApplySelectedTypeFilters();

    /** @brief Shows or hides the item details view. */
    void SetItemDetailsVisible(bool visible);

    // parser::IDataParserObserver
    void ProgressUpdated() override;
    void NewEventFound(db::LogEvent&& event) override;
    void NewEventBatchFound(std::vector<std::pair<int,
        db::LogEvent::EventItems>>&& eventBatch) override;

  private:
    static int ClampToInt(std::size_t value);

    IMainWindowView& m_view;
    mvc::IController& m_controller;
    db::EventsContainer& m_events;
    ui::ISearchResultsView& m_searchResultsView;
    ui::IEventsListView* m_eventsListView {nullptr};
    ui::ITypeFilterView* m_typeFilterView {nullptr};
    ui::IItemDetailsView* m_itemDetailsView {nullptr};
    bool m_isParsing {false};
    bool m_progressConfigured {false};
};

} // namespace ui
