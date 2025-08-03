/**
 * @file LogEvent.cpp
 * @brief Implementation of the LogEvent class methods.
 * @author LogViewer Development Team
 * @date 2025
 */

#include "db/LogEvent.hpp"
#include <regex>
#include <spdlog/spdlog.h>

namespace db
{

int LogEvent::getId() const
{
    spdlog::debug("LogEvent::getId called, returning {}", m_id);
    return m_id;
}

const LogEvent::EventItems& LogEvent::getEventItems() const
{
    spdlog::debug("LogEvent::getEventItems called, returning {} items",
        m_eventItems.size());
    return m_eventItems;
}

const std::string LogEvent::findByKey(const std::string& key) const
{
    spdlog::debug("LogEvent::findByKey called with key: {}", key);

    std::string result {""};
    auto pair = std::ranges::find_if(
        m_eventItems, [&key](const auto& item) { return item.first == key; });

    if (pair != m_eventItems.end())
    {
        result = pair->second;
        spdlog::debug("LogEvent::findByKey found value: {}", result);
    }
    else
    {
        spdlog::debug("LogEvent::findByKey did not find key: {}", key);
    }

    return result;
}

LogEvent::EventItemsIterator LogEvent::findInEvent(const std::string& search)
{
    spdlog::debug("LogEvent::findInEvent called with search: {}", search);
    std::regex searchRegex(search);
    auto it =
        std::ranges::find_if(m_eventItems, [&searchRegex](const auto& item)
            { return std::regex_search(item.second, searchRegex); });
    if (it != m_eventItems.end())
    {
        spdlog::debug(
            "LogEvent::findInEvent found match for search: {}", search);
    }
    else
    {
        spdlog::debug(
            "LogEvent::findInEvent found no match for search: {}", search);
    }
    return it;
}

} // namespace db
