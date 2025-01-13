#include "db/LogEvent.hpp"
#include <regex>

namespace db
{

  int LogEvent::getId() const
  {
    return m_id;
  }

  const LogEvent::EventItems &LogEvent::getEventItems() const
  {
    return m_eventItems;
  }

  const std::string LogEvent::findByKey(const std::string &key) const
  {

    std::string result{""};
    auto pair = std::ranges::find_if(m_eventItems, [&key](const auto &item)
                                     { return item.first == key; });

    if (pair != m_eventItems.end())
    {
      result = pair->second;
    }

    return result;
  }

  LogEvent::EventItemsIterator LogEvent::findInEvent(const std::string &search)
  {
    std::regex searchRegex(search);
    return std::ranges::find_if(m_eventItems, [&searchRegex](const auto &item)
                                { return std::regex_search(item.second, searchRegex); });
  }

} // namespace db
