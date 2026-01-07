/**
 * @file LogEvent.cpp
 * @brief Implementation of the LogEvent class methods.
 * @author LogViewer Development Team
 * @date 2025
 */

#include "LogEvent.hpp"
#include <regex>
#include "Logger.hpp"

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

const std::string LogEvent::findByKey(std::string_view key) const
{
    util::Logger::Trace("LogEvent::findByKey called with key: {}", std::string(key));

    // Use fast lookup index first
    auto it = m_lookupIndex.find(std::string(key));
    if (it != m_lookupIndex.end())
    {
        const auto& result = m_eventItems[it->second].second;
        util::Logger::Trace("LogEvent::findByKey found value: {}", result);
        return result;
    }

    // Fallback to linear search for edge cases (shouldn't happen with proper index)
    auto pair = std::ranges::find_if(
        m_eventItems, [key](const auto& item) { return item.first == key; });

    if (pair != m_eventItems.end())
    {
        util::Logger::Trace("LogEvent::findByKey found value (fallback): {}", pair->second);
        return pair->second;
    }
    else
    {
        util::Logger::Trace("LogEvent::findByKey did not find key: {}", std::string(key));
    }

    return "";
}

const std::vector<std::string> LogEvent::findAllByKey(std::string_view key) const {
    util::Logger::Trace("LogEvent::findAllByKey called with key: {}", std::string(key));

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



void LogEvent::buildLookupIndex()
{
    m_lookupIndex.clear();
    m_lookupIndex.reserve(m_eventItems.size());
    
    for (size_t i = 0; i < m_eventItems.size(); ++i)
    {
        // Only index the first occurrence of each key (maintains existing behavior)
        if (m_lookupIndex.find(m_eventItems[i].first) == m_lookupIndex.end())
        {
            m_lookupIndex[m_eventItems[i].first] = i;
        }
    }
}

void LogEvent::SetOriginalId(int originalId)
{
    // Store the original ID as a special field in the event data
    // Remove any existing original_id field first to avoid duplicates
    auto it = std::ranges::find_if(m_eventItems,
        [](const auto& item) { return item.first == "original_id"; });
    if (it != m_eventItems.end())
    {
        m_eventItems.erase(it);
    }
    
    // Add the original ID as a data field
    m_eventItems.push_back({"original_id", std::to_string(originalId)});
    
    // Rebuild the lookup index since we modified the items
    buildLookupIndex();
    
    util::Logger::Debug("LogEvent::SetOriginalId: Set original_id={} for event id={}", 
                        originalId, m_id);
}

void LogEvent::SetId(int newId)
{
    util::Logger::Debug("LogEvent::SetId: Updating event id from {} to {}", m_id, newId);
    m_id = newId;
}

} // namespace db
