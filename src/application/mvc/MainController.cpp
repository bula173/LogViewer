#include "mvc/MainController.hpp"

#include "config/Config.hpp"
#include "error/Error.hpp"
#include "parser/ParserFactory.hpp"
#include "util/WxWidgetsUtils.hpp"

#include <algorithm>
#include <filesystem>

namespace mvc {

MainController::MainController(db::EventsContainer& events)
    : m_events(events)
{
}

std::vector<std::string> MainController::GetSearchColumns() const
{
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
        columns = {"timestamp", "type", "info"};
    }

    return columns;
}

void MainController::SearchEvents(const std::string& query,
    const std::vector<std::string>& columns,
    const std::function<void(const SearchResultRow&)>& onResult,
    std::function<void(size_t, size_t)> progressCallback)
{
    const size_t total = m_events.Size();

    const bool hasQuery = !query.empty();

    for (size_t i = 0; i < total; ++i)
    {
        const auto& event =
            m_events.GetEvent(wx_utils::to_model_index(i));

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
}

void MainController::LoadLogFile(const std::filesystem::path& filepath,
    parser::IDataParserObserver* observer)
{
    if (filepath.empty())
    {
        throw error::Error("File path is empty.");
    }

    if (!std::filesystem::exists(filepath))
    {
        throw error::Error("File does not exist: " + filepath.string());
    }

    auto parserResult = parser::ParserFactory::CreateFromFile(filepath);
    if (!parserResult.isOk())
    {
        throw parserResult.error();
    }

    m_events.Clear();
    m_activeParser = parserResult.unwrap();

    m_activeParser->RegisterObserver(this);
    if (observer)
        m_activeParser->RegisterObserver(observer);

    try
    {
        m_activeParser->ParseData(filepath);
    }
    catch (...)
    {
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
