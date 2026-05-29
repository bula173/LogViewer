#include "MainController.hpp"

#include "Config.hpp"
#include "Error.hpp"
#include "Logger.hpp"
#include "ParserFactory.hpp"

#include <algorithm>
#include <filesystem>

namespace mvc {

/// Default column names used for search when no visible columns are configured.
static constexpr std::array kDefaultSearchColumns{"timestamp", "type", "info"};

MainController::MainController(db::EventsContainer& events)
    : m_events(events)
{
}

std::vector<std::string> MainController::GetSearchColumns() const
{
    util::Logger::Debug("GetSearchColumns: building visible column list");
    std::vector<std::string> columns;
    const auto& cfg = config::GetConfig();
    for (const auto& col : cfg.columns)
    {
        if (col.isVisible)
        {
            columns.push_back(col.name);
        }
    }

    if (columns.empty())
    {
        util::Logger::Debug("GetSearchColumns: no visible columns configured, using defaults");
        columns.assign(kDefaultSearchColumns.begin(), kDefaultSearchColumns.end());
    }

    util::Logger::Debug("GetSearchColumns: returning {} column(s)", columns.size());
    return columns;
}

void MainController::SearchEvents(const std::string& query,
    const std::vector<std::string>& columns,
    const std::function<void(const SearchResultRow&)>& onResult,
    std::function<void(size_t, size_t)> progressCallback)
{
    const size_t total = m_events.Size();
    util::Logger::Debug("SearchEvents: starting — query='{}', columns={}, total events={}",
        query, columns.size(), total);

    const bool hasQuery = !query.empty();
    size_t matchCount = 0;

    for (size_t i = 0; i < total; ++i)
    {
        const auto& event = m_events.GetEvent(i);

        bool matches = !hasQuery;
        std::string matchedText;
        std::string matchedKey;

        if (hasQuery)
        {
            for (const auto& item : event.getEventItems())
            {
                if (item.second.find(query) != std::string::npos)
                {
                    matchedText = item.second;
                    matchedKey = item.first;
                    matches = true;
                    break;
                }
            }
        }

        if (matches && onResult)
        {
            ++matchCount;
            util::Logger::Trace("SearchEvents: match at index={}, eventId={}, key='{}'",
                i, event.getId(), matchedKey);

            SearchResultRow row;
            row.eventId = event.getId();
            row.matchedKey = matchedKey;
            row.matchedText = matchedText;
            row.columnValues.reserve(columns.size());

            for (const auto& key : columns)
            {
                row.columnValues.push_back(event.findByKey(key));
            }

            onResult(row);
        }

        if (progressCallback)
        {
            progressCallback(i + 1, total);
        }
    }

    util::Logger::Info("SearchEvents: complete — {} match(es) out of {} event(s) for query='{}'",
        matchCount, total, query);
}

void MainController::LoadLogFile(const std::filesystem::path& filepath,
    parser::IDataParserObserver* observer)
{
    util::Logger::Debug("LoadLogFile: entry — path='{}'", filepath.string());

    if (filepath.empty())
    {
        util::Logger::Warn("LoadLogFile: rejected — file path is empty");
        throw error::Error(error::ErrorCode::InvalidArgument, "File path is empty.", false);
    }

    if (!std::filesystem::exists(filepath))
    {
        util::Logger::Warn("LoadLogFile: rejected — file does not exist: '{}'",
            filepath.string());
        throw error::Error(error::ErrorCode::FileNotFound,
            "File does not exist: " + filepath.string(), false);
    }

    util::Logger::Debug("LoadLogFile: creating parser for extension '{}'",
        filepath.extension().string());
    auto parserResult = parser::ParserFactory::CreateFromFile(filepath);
    if (!parserResult.isOk())
    {
        util::Logger::Warn("LoadLogFile: no parser found for '{}' — {}",
            filepath.string(), parserResult.error().what());
        throw parserResult.error();
    }

    util::Logger::Info("LoadLogFile: loading '{}' — parser ready, clearing existing events",
        filepath.string());
    m_events.Clear();
    m_activeParser = parserResult.unwrap();

    m_activeParser->RegisterObserver(this);
    if (observer)
        m_activeParser->RegisterObserver(observer);

    try
    {
        m_activeParser->ParseData(filepath);
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("LoadLogFile: parse failed for '{}' — {}",
            filepath.string(), ex.what());
        if (observer)
            m_activeParser->UnregisterObserver(observer);
        m_activeParser->UnregisterObserver(this);
        m_activeParser.reset();
        throw;
    }
    catch (...)
    {
        util::Logger::Error("LoadLogFile: parse failed for '{}' — unknown exception",
            filepath.string());
        if (observer)
            m_activeParser->UnregisterObserver(observer);
        m_activeParser->UnregisterObserver(this);
        m_activeParser.reset();
        throw;
    }

    if (observer)
        m_activeParser->UnregisterObserver(observer);
    m_activeParser->UnregisterObserver(this);
    m_activeParser.reset();
    util::Logger::Info("LoadLogFile: complete — '{}', {} event(s) loaded",
        filepath.string(), m_events.Size());
}

uint32_t MainController::GetParserCurrentProgress() const
{
    return m_activeParser ? m_activeParser->GetCurrentProgress() : 0;
}

uint32_t MainController::GetParserTotalProgress() const
{
    return m_activeParser ? m_activeParser->GetTotalProgress() : 0;
}

void MainController::ProgressUpdated()
{
    // No-op for now; UI observers receive updates directly from the parser.
}

void MainController::NewEventFound(db::LogEvent&& event)
{
    m_events.AddEvent(std::move(event));
}

void MainController::NewEventBatchFound(
    std::vector<std::pair<int, db::LogEvent::EventItems>>&& eventBatch)
{
    m_events.AddEventBatch(std::move(eventBatch));
}

} // namespace mvc
