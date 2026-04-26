#include "MainWindowPresenter.hpp"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <exception>
#include <filesystem>
#include <limits>
#include <set>
#include <string>
#include <thread>
#include <vector>

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
    clearAllData();                  // notifies views on main thread - OK
    m_view.SetSearchControlsEnabled(false);
    m_view.ToggleProgressVisibility(true);
    m_view.ConfigureProgressRange(100);
    m_view.UpdateProgressValue(0);
    m_view.UpdateStatusText("Loading ...");

    // Create parser on main thread before handing off to background
    auto parserResult = parser::ParserFactory::CreateFromFile(path);
    if (!parserResult.isOk())
    {
        m_isParsing = false;
        m_progressConfigured = false;
        m_view.ToggleProgressVisibility(false);
        m_view.SetSearchControlsEnabled(true);
        m_view.UpdateStatusText(previousStatus);
        throw parserResult.error();
    }
    auto parser = parserResult.unwrap();

    // Suppress per-batch view notifications while the background thread fills
    // m_events.  Without this, every batch would call Qt widget methods from
    // the background thread (undefined behaviour) AND trigger dozens of full
    // model resets.  A single RefreshView() is done after parsing completes.
    m_events.SuspendNotifications();

    // Observer that runs entirely on the background thread:
    //  - adds events to m_events via its thread-safe mutex
    //  - stores progress in atomics so the main thread can read it safely
    struct BgObserver final : public parser::IDataParserObserver
    {
        db::EventsContainer& events;
        const parser::IDataParser* parserPtr;
        std::atomic<uint32_t> currentProgress{0};
        std::atomic<uint32_t> totalProgress{0};

        BgObserver(db::EventsContainer& ev, const parser::IDataParser* p)
            : events(ev), parserPtr(p) {}

        void ProgressUpdated() override
        {
            // Background thread: safe to read parser state here
            currentProgress.store(parserPtr->GetCurrentProgress(),
                                  std::memory_order_relaxed);
            totalProgress.store(parserPtr->GetTotalProgress(),
                                std::memory_order_relaxed);
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

    BgObserver bgObserver{m_events, parser.get()};
    parser->RegisterObserver(&bgObserver);

    std::exception_ptr parseException;
    std::atomic<bool> parsingDone{false};

    std::thread parseThread(
        [&parser, &path, &parseException, &parsingDone]()
        {
            try
            {
                parser->ParseData(path);
            }
            catch (...)
            {
                parseException = std::current_exception();
            }
            parsingDone.store(true, std::memory_order_release);
        });

    // Main thread: keep UI responsive at ~60 Hz while background parses
    {
        uint32_t lastDisplayedProgress = 0;
        while (!parsingDone.load(std::memory_order_acquire))
        {
            m_view.ProcessPendingEvents();

            const uint32_t total =
                bgObserver.totalProgress.load(std::memory_order_relaxed);
            const uint32_t current =
                bgObserver.currentProgress.load(std::memory_order_relaxed);

            if (total > 0 && !m_progressConfigured)
            {
                m_view.ConfigureProgressRange(ClampToInt(total));
                m_progressConfigured = true;
            }
            if (current != lastDisplayedProgress)
            {
                m_view.UpdateProgressValue(ClampToInt(current));
                lastDisplayedProgress = current;
            }

            std::this_thread::sleep_for(std::chrono::milliseconds(16));
        }
    }

    parseThread.join();
    m_events.ResumeNotifications();

    if (parseException)
    {
        m_isParsing = false;
        m_progressConfigured = false;
        m_view.ToggleProgressVisibility(false);
        m_view.SetSearchControlsEnabled(true);
        m_view.UpdateStatusText(previousStatus);
        UpdateTypeFilters();
        if (m_eventsListView)
            m_eventsListView->RefreshView();
        if (m_itemDetailsView)
            m_itemDetailsView->RefreshView();
        std::rethrow_exception(parseException);
    }

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

void MainWindowPresenter::MergeLogFile(const std::filesystem::path& path,
                                       const std::string& existingAlias,
                                       const std::string& newFileAlias,
                                       const std::string& timestampField)
{
    if (m_isParsing)
        throw error::Error("A file is already being processed");

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
        auto parserResult = parser::ParserFactory::CreateFromFile(path);
        if (!parserResult.isOk())
        {
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
        
        // Now merge the temporary container into main events
        // Pass both aliases: existing data gets existingAlias, new data gets newFileAlias
        m_events.MergeEvents(tempEvents, existingAlias, newFileAlias, timestampField);
        
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
    catch (...)
    {
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

    const std::string previousStatus = m_view.CurrentStatusText();
    m_view.UpdateStatusText("Updating filters...");

    auto& config = config::GetConfig();
    std::set<std::string> types;
    const std::size_t total = m_events.Size();
    for (std::size_t i = 0; i < total; ++i)
    {
        const auto& event = m_events.GetEvent(ClampToInt(i));
        types.insert(event.findByKey(config.typeFilterField));

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

    auto& config = config::GetConfig();
    const auto checkedTypes = m_typeFilterView->CheckedTypes();
    const std::set<std::string> selectedTypeStrings(
        checkedTypes.begin(), checkedTypes.end());

    std::vector<unsigned long> filteredIndices;
    filteredIndices.reserve(m_events.Size());

    const std::size_t total = m_events.Size();
    for (std::size_t i = 0; i < total; ++i)
    {
        const auto& event = m_events.GetEvent(ClampToInt(i));
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
