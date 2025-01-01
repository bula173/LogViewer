#include "db/event.hpp"
#include <regex>

namespace db
{

  Event::Event(const int id, EventItems &&eventItems)
      : m_id(id), m_events(eventItems)
  {
  }

  int Event::getId() const
  {
    return m_id;
  }

  const Event::EventItems &Event::getEventItems() const
  {
    return m_events;
  }

  const std::string Event::findByKey(const std::string &key) const
  {

    return std::ranges::find_if(m_events, [&key](const auto &item)
                                { return item.first == key; })
        ->second;
  }

  Event::EventItemsIterator Event::findInEvent(const std::string &search)
  {
    std::regex searchRegex(search);
    return std::ranges::find_if(m_events, [&searchRegex](const auto &item)
                                { return std::regex_search(item.second, searchRegex); });
  }

} // namespace db
