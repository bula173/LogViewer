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
    /**
     * @brief Linear search through event items to find matching key.
     *
     * Uses std::find_if with a lambda to locate the first item with matching
     * key. Returns empty string if no match is found.
     */
    auto it = std::find_if(m_eventItems.begin(), m_eventItems.end(),
        [&key](const std::pair<std::string, std::string>& element)
        { return element.first == key; });

    if (it != m_eventItems.end())
    {
        return it->second;
    }
    return "";
}

LogEvent::EventItemsIterator LogEvent::findInEvent(const std::string& search)
{
    /**
     * @brief Searches for the given term in both keys and values.
     *
     * Performs substring search across all key-value pairs in the event.
     * Returns iterator to first matching item, or end() if not found.
     */
    return std::find_if(m_eventItems.begin(), m_eventItems.end(),
        [&search](const std::pair<std::string, std::string>& element)
        {
            // Search in both key and value using substring matching
            return element.first.find(search) != std::string::npos ||
                element.second.find(search) != std::string::npos;
        });
}

} // namespace db
