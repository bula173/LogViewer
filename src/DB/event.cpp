#include "event.hpp"

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

  const std::string Event::find(const std::string &key) const
  {
    for (auto &it : m_events)
    {
      if (it.first == key)
      {
        return it.second;
      }
    }
    return "";
  }

} // namespace db
