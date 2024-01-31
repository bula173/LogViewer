#include "event.hpp"

namespace db
{

  Event::Event(const int id, EventItems &&eventItems)
      : m_id(id)
      , m_events(eventItems)
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

} // namespace db
