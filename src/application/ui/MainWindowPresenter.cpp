#include "ui/MainWindowPresenter.hpp"

#include <algorithm>
#include <cstddef>
#include <filesystem>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include "error/Error.hpp"
#include "util/Logger.hpp"

namespace ui
{
namespace
{
constexpr std::size_t kProgressYieldInterval = 500;
}

MainWindowPresenter::MainWindowPresenter(IMainWindowView& view,
    mvc::IController& controller, db::EventsContainer& events,
    ui::ISearchResultsView& searchResults, ui::IEventsListView* eventsListView,
    ui::ITypeFilterView* typeFilterView, ui::IItemDetailsView* itemDetailsView)
    : m_view(view)
    , m_controller(controller)
    , m_events(events)
    , m_searchResultsView(searchResults)
    , m_eventsListView(eventsListView)
    , m_typeFilterView(typeFilterView)
    , m_itemDetailsView(itemDetailsView)
{
}

int MainWindowPresenter::ClampToInt(std::size_t value)
{
    const auto maxInt = static_cast<std::size_t>(
        std::numeric_limits<int>::max());
    return static_cast<int>(std::min(value, maxInt));
}

void MainWindowPresenter::PerformSearch()
{
    const auto columns = m_controller.GetSearchColumns();
    m_searchResultsView.BeginUpdate(columns);

    const std::string previousStatus = m_view.CurrentStatusText();
    m_view.UpdateStatusText("Searching ...");

    const std::size_t total = m_events.Size();
    const int range = ClampToInt(std::max<std::size_t>(total, 1));
    m_view.ToggleProgressVisibility(true);
    m_view.ConfigureProgressRange(range);
    m_view.UpdateProgressValue(0);
    m_view.SetSearchControlsEnabled(false);

    const auto query = m_view.ReadSearchQuery();

    auto appendResult = [this](const mvc::SearchResultRow& result) {
        m_searchResultsView.AppendResult(result);
    };

    auto progressCallback = [this](std::size_t processed, std::size_t) {
        m_view.UpdateProgressValue(ClampToInt(processed));
        if ((processed % kProgressYieldInterval) == 0)
            m_view.ProcessPendingEvents();
    };

    try
    {
        m_controller.SearchEvents(
            query, columns, appendResult, progressCallback);

        m_view.UpdateProgressValue(range);
        m_view.ToggleProgressVisibility(false);
        m_view.SetSearchControlsEnabled(true);
        m_view.UpdateStatusText(previousStatus);
        m_searchResultsView.EndUpdate();
        m_view.ProcessPendingEvents();
    }
    catch (...)
    {
        m_view.ToggleProgressVisibility(false);
        m_view.SetSearchControlsEnabled(true);
        m_searchResultsView.EndUpdate();
        throw;
    }
}

void MainWindowPresenter::LoadLogFile(const std::filesystem::path& path)
{
    if (m_isParsing)
        throw error::Error("A file is already being processed");

    const std::string previousStatus = m_view.CurrentStatusText();
    m_isParsing = true;
    m_progressConfigured = false;
    m_view.UpdateStatusText("Clear...");
    clearAllData();
    m_view.SetSearchControlsEnabled(false);
    m_view.ToggleProgressVisibility(true);
    m_view.ConfigureProgressRange(100);
    m_view.UpdateProgressValue(0);
    m_view.UpdateStatusText("Loading ...");

    try
    {
        m_controller.LoadLogFile(path, this);
        m_view.UpdateStatusText("Data ready. Path: " + path.string());
        m_view.ToggleProgressVisibility(false);
        m_view.SetSearchControlsEnabled(true);
        UpdateTypeFilters();
        if (m_eventsListView)
            m_eventsListView->RefreshView();
        if (m_itemDetailsView)
            m_itemDetailsView->RefreshView();
        m_isParsing = false;
        m_progressConfigured = false;
        m_view.ProcessPendingEvents();
    }
    catch (...)
    {
        m_view.ToggleProgressVisibility(false);
        m_view.SetSearchControlsEnabled(true);
        m_view.UpdateStatusText(previousStatus);
        
        // Update type filters even on error, so any successfully parsed events are shown
        UpdateTypeFilters();
        if (m_eventsListView)
            m_eventsListView->RefreshView();
        if (m_itemDetailsView)
            m_itemDetailsView->RefreshView();
        
        m_isParsing = false;
        m_progressConfigured = false;
        throw;
    }
}

void MainWindowPresenter::UpdateTypeFilters()
{
    if (!m_typeFilterView)
        return;

    const std::string previousStatus = m_view.CurrentStatusText();
    m_view.UpdateStatusText("Updating filters...");

    std::set<std::string> types;
    const std::size_t total = m_events.Size();
    for (std::size_t i = 0; i < total; ++i)
    {
        const auto& event = m_events.GetEvent(ClampToInt(i));
        types.insert(event.findByKey("type"));

        if ((i % kProgressYieldInterval) == 0)
            m_view.ProcessPendingEvents();
    }

    std::vector<std::string> ordered(types.begin(), types.end());
    m_typeFilterView->ReplaceTypes(ordered, true);
    m_typeFilterView->ShowControl(true);

    m_view.UpdateStatusText(previousStatus);
    m_view.RefreshLayout();
}

void MainWindowPresenter::ApplySelectedTypeFilters()
{
    if (!m_typeFilterView)
    {
        util::Logger::Warn(
            "Type filter view not initialized; cannot apply type filters.");
        return;
    }

    if (!m_eventsListView)
    {
        util::Logger::Warn(
            "Events list view not initialized; cannot apply type filters.");
        return;
    }

    const std::string previousStatus = m_view.CurrentStatusText();
    m_view.UpdateStatusText("Applying filters...");

    const auto checkedTypes = m_typeFilterView->CheckedTypes();
    const std::set<std::string> selectedTypeStrings(
        checkedTypes.begin(), checkedTypes.end());

    std::vector<unsigned long> filteredIndices;
    filteredIndices.reserve(m_events.Size());

    const std::size_t total = m_events.Size();
    for (std::size_t i = 0; i < total; ++i)
    {
        const auto& event = m_events.GetEvent(ClampToInt(i));
        const std::string eventType = event.findByKey("type");
        const bool typeMatch = selectedTypeStrings.empty() ||
            selectedTypeStrings.count(eventType) > 0;

        if (typeMatch)
            filteredIndices.push_back(i);

        if ((i % kProgressYieldInterval) == 0)
            m_view.ProcessPendingEvents();
    }

    if (filteredIndices.empty())
    {
        util::Logger::Info("No events match the selected filters.");
    }
    else
    {
        util::Logger::Info(
            "Filtered events count: {}", filteredIndices.size());
    }

    m_eventsListView->SetFilteredEvents(filteredIndices);
    m_eventsListView->RefreshView();

    m_view.UpdateStatusText(previousStatus);
}

void MainWindowPresenter::SetItemDetailsVisible(bool visible)
{
    if (!m_itemDetailsView)
        return;

    m_itemDetailsView->ShowControl(visible);
    m_view.RefreshLayout();
}

void MainWindowPresenter::ProgressUpdated()
{
    const auto total = m_controller.GetParserTotalProgress();
    if (!m_progressConfigured && total > 0)
    {
        m_view.ConfigureProgressRange(ClampToInt(total));
        m_progressConfigured = true;
    }

    m_view.UpdateProgressValue(ClampToInt(m_controller.GetParserCurrentProgress()));
    m_view.ProcessPendingEvents();
}

void MainWindowPresenter::NewEventFound(db::LogEvent&&)
{
    // Controller already stores the event; UI refresh happens on batches.
}

void MainWindowPresenter::NewEventBatchFound(
    std::vector<std::pair<int, db::LogEvent::EventItems>>&&)
{
    if (m_eventsListView)
        m_eventsListView->RefreshView();
    if (m_itemDetailsView)
        m_itemDetailsView->RefreshView();
}

void MainWindowPresenter::clearAllData()
{
    util::Logger::Debug("Clearing all data from presenter-managed components");
    m_searchResultsView.Clear();
    m_events.Clear();
    util::Logger::Debug("All data cleared successfully");
}

} // namespace ui
