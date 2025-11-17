#include "mvc/MainController.hpp"

#include "config/Config.hpp"
#include "util/WxWidgetsUtils.hpp"

#include <algorithm>

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

        if (hasQuery)
        {
            for (const auto& item : event.getEventItems())
            {
                if (item.second.find(query) != std::string::npos)
                {
                    matchedText = item.second;
                    matches = true;
                    break;
                }
            }
        }

        if (matches && onResult)
        {
            SearchResultRow row;
            row.eventId = event.getId();
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

} // namespace mvc
