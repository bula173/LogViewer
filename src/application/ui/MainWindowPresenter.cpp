#include "MainWindowPresenter.hpp"

#include <algorithm>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <limits>
#include <set>
#include <string>
#include <vector>

#include <QCoreApplication>
#include <QEventLoop>

#include "Config.hpp"
#include "Error.hpp"
#include "Logger.hpp"
#include "ParserFactory.hpp"

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
    util::Logger::Debug("PerformSearch: starting search");
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

    std::size_t resultCount = 0;
    auto appendResult = [this, &resultCount](const mvc::SearchResultRow& result) {
        m_searchResultsView.AppendResult(result);
        ++resultCount;
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
        util::Logger::Info("PerformSearch: complete — {} result(s) for query='{}'",
            resultCount, query);
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("PerformSearch: exception during search — {}", ex.what());
        m_view.ToggleProgressVisibility(false);
        m_view.SetSearchControlsEnabled(true);
        m_searchResultsView.EndUpdate();
        throw;
    }
    catch (...)
    {
        util::Logger::Error("PerformSearch: unknown exception during search");
        m_view.ToggleProgressVisibility(false);
        m_view.SetSearchControlsEnabled(true);
        m_searchResultsView.EndUpdate();
        throw;
    }
}

void MainWindowPresenter::LoadLogFile(const std::filesystem::path& path)
{
    util::Logger::Info("LoadLogFile: requested — path='{}'", path.string());

    if (m_isParsing)
    {
        util::Logger::Warn("LoadLogFile: rejected — a file is already being processed");
        throw error::Error("A file is already being processed");
    }

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

    util::Logger::Debug("LoadLogFile: creating parser for extension '{}'",
        path.extension().string());
    auto parserResult = parser::ParserFactory::CreateFromFile(path);
    if (!parserResult.isOk())
    {
        util::Logger::Warn("LoadLogFile: no parser available for '{}' — {}",
            path.string(), parserResult.error().what());
        m_isParsing = false;
        m_progressConfigured = false;
        m_view.ToggleProgressVisibility(false);
        m_view.SetSearchControlsEnabled(true);
        m_view.UpdateStatusText(previousStatus);
        throw parserResult.error();
    }

    util::Logger::Debug("LoadLogFile: dispatching async parser for '{}'", path.string());
    RunParserAsync(parserResult.unwrap(), path, previousStatus);
}

void MainWindowPresenter::LoadLogFile(std::unique_ptr<parser::IDataParser> parser,
                                      const std::filesystem::path& path)
{
    util::Logger::Info("LoadLogFile (pre-built parser): requested — path='{}'", path.string());

    if (m_isParsing)
    {
        util::Logger::Warn("LoadLogFile (pre-built parser): rejected — a file is already being processed");
        throw error::Error("A file is already being processed");
    }

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

    util::Logger::Debug("LoadLogFile (pre-built parser): dispatching async parser for '{}'",
        path.string());
    RunParserAsync(std::move(parser), path, previousStatus);
}

void MainWindowPresenter::RunParserAsync(std::unique_ptr<parser::IDataParser> parser,
                                         const std::filesystem::path& path,
                                         const std::string& previousStatus)
{
    util::Logger::Debug("RunParserAsync: dispatching background parse task for '{}'",
        path.string());

    // Suppress per-batch view notifications while parsing so the single
    // RefreshView() at the end triggers one clean model reset.
    m_events.SuspendNotifications();

    // Cooperative observer: parsing runs on the main thread; ProgressUpdated()
    // pumps the Qt event loop (excluding user input to prevent re-entrant loads)
    // so the progress bar stays responsive without any background threads.
    struct CoopObserver final : public parser::IDataParserObserver
    {
        db::EventsContainer&       events;
        const parser::IDataParser* parserPtr;
        IMainWindowView&           view;
        bool&                      progressConfigured;

        CoopObserver(db::EventsContainer& ev, const parser::IDataParser* p,
                     IMainWindowView& v, bool& pc)
            : events(ev), parserPtr(p), view(v), progressConfigured(pc) {}

        void ProgressUpdated() override
        {
            const auto total   = parserPtr->GetTotalProgress();
            const auto current = parserPtr->GetCurrentProgress();
            if (total > 0 && !progressConfigured) {
                view.ConfigureProgressRange(static_cast<int>(total));
                progressConfigured = true;
            }
            view.UpdateProgressValue(static_cast<int>(current));
            QCoreApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }

        void NewEventFound(db::LogEvent&& e) override
        {
            events.AddEvent(std::move(e));
        }

        void NewEventBatchFound(
            std::vector<std::pair<int, db::LogEvent::EventItems>>&& batch) override
        {
            events.AddEventBatch(std::move(batch));
        }
    };

    CoopObserver coopObserver{m_events, parser.get(), m_view, m_progressConfigured};
    parser->RegisterObserver(&coopObserver);

    std::exception_ptr parseException;
    try
    {
        parser->ParseData(path);
    }
    catch (...)
    {
        parseException = std::current_exception();
    }

    parser->UnregisterObserver(&coopObserver);
    m_events.ResumeNotifications();

    if (parseException)
    {
        try
        {
            std::rethrow_exception(parseException);
        }
        catch (const std::exception& ex)
        {
            util::Logger::Error("RunParserAsync: parse task failed for '{}' — {}",
                path.string(), ex.what());
        }
        catch (...)
        {
            util::Logger::Error("RunParserAsync: parse task failed for '{}' — unknown exception",
                path.string());
        }

        m_isParsing = false;
        m_progressConfigured = false;
        m_view.ToggleProgressVisibility(false);
        m_view.SetSearchControlsEnabled(true);
        m_view.UpdateStatusText(previousStatus);
        UpdateTypeFilters();
        if (m_itemDetailsView)
            m_itemDetailsView->RefreshView();
        std::rethrow_exception(parseException);
    }

    const std::size_t eventCount = m_events.Size();
    util::Logger::Info("RunParserAsync: parse complete — '{}', {} event(s) loaded",
        path.string(), eventCount);

    if (m_eventsListView)
    {
        m_eventsListView->ClearFilter();
        m_eventsListView->RefreshView();
    }
    if (m_itemDetailsView)
        m_itemDetailsView->RefreshView();

    m_view.UpdateStatusText("Data ready. Path: " + path.string());
    m_view.ToggleProgressVisibility(false);
    m_view.SetSearchControlsEnabled(true);
    m_view.ProcessPendingEvents();

    UpdateTypeFilters();
    m_isParsing = false;
    m_progressConfigured = false;
}

void MainWindowPresenter::MergeLogFile(const std::filesystem::path& path,
                                       const std::string& existingAlias,
                                       const std::string& newFileAlias,
                                       const std::string& timestampField)
{
    util::Logger::Info("MergeLogFile: start — path='{}', existingAlias='{}', newAlias='{}'",
        path.string(), existingAlias, newFileAlias);

    if (m_isParsing)
    {
        util::Logger::Warn("MergeLogFile: rejected — a file is already being processed");
        throw error::Error("A file is already being processed");
    }

    const std::string previousStatus = m_view.CurrentStatusText();
    m_isParsing = true;
    m_progressConfigured = false;

    // Create temporary container for new events
    db::EventsContainer tempEvents;

    m_view.SetSearchControlsEnabled(false);
    m_view.ToggleProgressVisibility(true);
    m_view.ConfigureProgressRange(100);
    m_view.UpdateProgressValue(0);
    m_view.UpdateStatusText("Merging ...");

    try
    {
        // Parse into temporary container using a temporary observer
        // For now, we'll use a simpler approach: parse and collect events manually
        util::Logger::Debug("MergeLogFile: creating parser for extension '{}'",
            path.extension().string());
        auto parserResult = parser::ParserFactory::CreateFromFile(path);
        if (!parserResult.isOk())
        {
            util::Logger::Warn("MergeLogFile: no parser available for '{}' — {}",
                path.string(), parserResult.error().what());
            throw parserResult.error();
        }

        auto parser = parserResult.unwrap();

        // Register a simple observer that adds events to tempEvents
        class TempObserver : public parser::IDataParserObserver
        {
        public:
            db::EventsContainer& container;
            explicit TempObserver(db::EventsContainer& c) : container(c) {}
            void ProgressUpdated() override {}
            void NewEventFound(db::LogEvent&& event) override
            {
                container.AddEvent(std::move(event));
            }
            void NewEventBatchFound(std::vector<std::pair<int, db::LogEvent::EventItems>>&& eventBatch) override
            {
                container.AddEventBatch(std::move(eventBatch));
            }
        };

        TempObserver tempObserver(tempEvents);
        parser->RegisterObserver(&tempObserver);

        parser->ParseData(path);

        parser->UnregisterObserver(&tempObserver);

        util::Logger::Debug("MergeLogFile: parsed {} event(s) from '{}', merging into main container",
            tempEvents.Size(), path.string());

        // Now merge the temporary container into main events
        // Pass both aliases: existing data gets existingAlias, new data gets newFileAlias
        m_events.MergeEvents(tempEvents, existingAlias, newFileAlias, timestampField);

        util::Logger::Info("MergeLogFile: complete — '{}', total {} event(s) after merge",
            path.string(), m_events.Size());

        m_view.UpdateStatusText("Merge complete. Path: " + path.string());
        m_view.ToggleProgressVisibility(false);
        m_view.SetSearchControlsEnabled(true);
        UpdateTypeFilters();
        if (m_eventsListView)
        {
            m_eventsListView->RefreshColumns(); // Refresh columns to show source column
            m_eventsListView->RefreshView();
        }
        if (m_itemDetailsView)
            m_itemDetailsView->RefreshView();
        m_isParsing = false;
        m_progressConfigured = false;
        m_view.ProcessPendingEvents();
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("MergeLogFile: failed for '{}' — {}", path.string(), ex.what());
        m_view.ToggleProgressVisibility(false);
        m_view.SetSearchControlsEnabled(true);
        m_view.UpdateStatusText(previousStatus);

        UpdateTypeFilters();
        if (m_eventsListView)
        {
            m_eventsListView->RefreshColumns(); // Refresh columns even on error
            m_eventsListView->RefreshView();
        }
        if (m_itemDetailsView)
            m_itemDetailsView->RefreshView();

        m_isParsing = false;
        m_progressConfigured = false;
        throw;
    }
    catch (...)
    {
        util::Logger::Error("MergeLogFile: failed for '{}' — unknown exception", path.string());
        m_view.ToggleProgressVisibility(false);
        m_view.SetSearchControlsEnabled(true);
        m_view.UpdateStatusText(previousStatus);

        UpdateTypeFilters();
        if (m_eventsListView)
        {
            m_eventsListView->RefreshColumns(); // Refresh columns even on error
            m_eventsListView->RefreshView();
        }
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

    util::Logger::Debug("UpdateTypeFilters: rebuilding type filter list from {} event(s)",
        m_events.Size());

    const std::string previousStatus = m_view.CurrentStatusText();
    m_view.UpdateStatusText("Updating filters...");

    auto& config = config::GetConfig();

    // If the user has an explicit preference (saved in config), use it directly.
    // If the field is empty, try auto-detection; if that also fails, ask the user.
    if (config.typeFilterField.empty())
    {
        static constexpr const char* kTypeFieldCandidates[] = {
            "level", "type", "severity", "priority", "category",
            "loglevel", "log_level", "logLevel", "eventtype", "event_type"
        };
        const std::size_t sampleSize = std::min(m_events.Size(), std::size_t{100});
        bool detected = false;
        for (const char* candidate : kTypeFieldCandidates)
        {
            for (std::size_t i = 0; i < sampleSize; ++i)
            {
                if (!m_events.GetEvent(i).findByKey(candidate).empty())
                {
                    config.typeFilterField = candidate;
                    util::Logger::Info("UpdateTypeFilters: auto-detected type field '{}'", candidate);
                    detected = true;
                    break;
                }
            }
            if (detected) break;
        }
        if (!detected)
        {
            util::Logger::Warn("UpdateTypeFilters: no known type field found in first {} events",
                sampleSize);
            bool ok = false;
            const std::string field = m_view.AskString(
                "Type Filter Field",
                "No type/level field was detected in the loaded log events.\n"
                "Enter the field name to use for type filtering\n"
                "(e.g. level, type, severity):",
                {}, ok);
            if (ok && !field.empty())
            {
                config.typeFilterField = field;
                util::Logger::Info("UpdateTypeFilters: user provided type field '{}'", field);
                config.SaveConfig();
            }
        }
    }
    else
    {
        util::Logger::Debug("UpdateTypeFilters: using configured type field '{}'",
            config.typeFilterField);
    }

    std::set<std::string> types;
    const std::size_t total = m_events.Size();
    for (std::size_t i = 0; i < total; ++i)
    {
        const auto& event = m_events.GetEvent(i);
        types.insert(event.findByKey(config.typeFilterField));

        if ((i % kProgressYieldInterval) == 0)
            m_view.ProcessPendingEvents();
    }

    std::vector<std::string> ordered(types.begin(), types.end());
    util::Logger::Debug("UpdateTypeFilters: found {} distinct type(s)", ordered.size());
    m_typeFilterView->ReplaceTypes(ordered, true);
    m_typeFilterView->ShowControl(true);

    m_view.UpdateStatusText(previousStatus);
    m_view.RefreshLayout();

    // Apply the filter immediately so the table reflects the initial
    // checked state (all types checked = show all events).
    ApplySelectedTypeFilters();
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

    auto& config = config::GetConfig();
    const auto checkedTypes = m_typeFilterView->CheckedTypes();
    const std::set<std::string> selectedTypeStrings(
        checkedTypes.begin(), checkedTypes.end());

    std::vector<unsigned long> filteredIndices;
    filteredIndices.reserve(m_events.Size());

    const std::size_t total = m_events.Size();
    for (std::size_t i = 0; i < total; ++i)
    {
        const auto& event = m_events.GetEvent(i);
        const std::string eventType = event.findByKey(config.typeFilterField);
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

    // If every event passes the filter, use the O(1) unfiltered path instead
    // of storing and indexing an n-element identity vector.
    if (filteredIndices.size() == m_events.Size())
    {
        m_eventsListView->ClearFilter();
    }
    else
    {
        m_eventsListView->SetFilteredEvents(filteredIndices);
    }
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
    // This method is only called in the synchronous (non-background-thread)
    // loading path (e.g. MergeLogFile's TempObserver). During async LoadLogFile
    // the presenter is not registered as a parser observer; progress is handled
    // by the polling loop in LoadLogFile instead.
    const auto total = m_controller.GetParserTotalProgress();
    if (!m_progressConfigured && total > 0)
    {
        m_view.ConfigureProgressRange(ClampToInt(total));
        m_progressConfigured = true;
    }
    m_view.UpdateProgressValue(ClampToInt(m_controller.GetParserCurrentProgress()));
    // Throttle event processing: only yield to the event loop every ~5 %
    // progress to avoid excessive overhead on large files.
    if (total > 0)
    {
        const auto current = m_controller.GetParserCurrentProgress();
        static uint32_t lastYield = 0;
        if ((current - lastYield) * 20 >= total)
        {
            lastYield = current;
            m_view.ProcessPendingEvents();
        }
    }
}

void MainWindowPresenter::NewEventFound(db::LogEvent&&)
{
    // Controller already stores the event; UI refresh happens on batches.
}

void MainWindowPresenter::NewEventBatchFound(
    std::vector<std::pair<int, db::LogEvent::EventItems>>&&)
{
    // During async LoadLogFile the presenter is not registered as observer so
    // this is never called from the background thread. For any synchronous
    // path that does call this (e.g. custom observers), guard against
    // refreshing during an active load.
    if (m_isParsing)
        return;

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
