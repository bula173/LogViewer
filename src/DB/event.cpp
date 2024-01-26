#include "event.hpp"

namespace db
{

  Event::Event(const int id)
      : m_id(id)
  {
  }

  int Event::getId() const
  {
    return m_id;
  }

} // namespace db
