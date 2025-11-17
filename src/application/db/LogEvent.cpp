/**
 * @file LogEvent.cpp
 * @brief Implementation of the LogEvent class methods.
 * @author LogViewer Development Team
 * @date 2025
 */

#include "db/LogEvent.hpp"
#include <regex>
#include "util/Logger.hpp"

namespace db
{

int LogEvent::getId() const
{
    util::Logger::Trace("LogEvent::getId called, returning {}", m_id);
    return m_id;
}

const LogEvent::EventItems& LogEvent::getEventItems() const
{
    util::Logger::Trace("LogEvent::getEventItems called, returning {} items",
        m_eventItems.size());
    return m_eventItems;
}

const std::string LogEvent::findByKey(const std::string& key) const
{
    util::Logger::Trace("LogEvent::findByKey called with key: {}", key);

    std::string result {""};
    auto pair = std::ranges::find_if(
        m_eventItems, [&key](const auto& item) { return item.first == key; });

    if (pair != m_eventItems.end())
    {
        result = pair->second;
        util::Logger::Trace("LogEvent::findByKey found value: {}", result);
    }
    else
    {
        util::Logger::Trace("LogEvent::findByKey did not find key: {}", key);
    }

    return result;
}

const std::vector<std::string> LogEvent::findAllByKey(const std::string& key) const {
    util::Logger::Trace("LogEvent::findAllByKey called with key: {}", key);

    std::vector<std::string> results;
    for (const auto& item : m_eventItems) {
        if (item.first == key) {
            results.push_back(item.second);
            util::Logger::Trace("LogEvent::findAllByKey found value: {}", item.second);
        }
    }

    if (results.empty()) {
        util::Logger::Trace("LogEvent::findAllByKey did not find key: {}", key);
    }

    return results;
}

LogEvent::EventItemsIterator LogEvent::findInEvent(const std::string& search)
{
    util::Logger::Trace("LogEvent::findInEvent called with search: {}", search);
    std::regex searchRegex(search);
    auto it =
        std::ranges::find_if(m_eventItems, [&searchRegex](const auto& item)
            { return std::regex_search(item.second, searchRegex); });
    if (it != m_eventItems.end())
    {
        util::Logger::Trace(
            "LogEvent::findInEvent found match for search: {}", search);
    }
    else
    {
        util::Logger::Trace(
            "LogEvent::findInEvent found no match for search: {}", search);
    }
    return it;
}



} // namespace db
