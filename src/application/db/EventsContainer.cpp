#include "EventsContainer.hpp"

namespace db
{
  EventsContainer::EventsContainer()
  {
  }

  EventsContainer::~EventsContainer()
  {
  }

  void EventsContainer::AddEvent(LogEvent &&event)
  {
    this->AddItem(std::move(event));
  }

  const LogEvent &EventsContainer::GetEvent(const int index)
  {
    return this->GetItem(index);
  }

  int EventsContainer::GetCurrentItemIndex()
  {
    return m_currentItem;
  }

  void EventsContainer::SetCurrentItem(const int item)
  {
    m_currentItem = item;
    for (auto v : m_views)
    {
      v->OnCurrentIndexUpdated(item);
    }
  }

  void EventsContainer::AddItem(LogEvent &&item)
  {
    m_data.push_back(std::forward<decltype(item)>(item));
    this->NotifyDataChanged();
  }

  LogEvent &EventsContainer::GetItem(const int index)
  {
    return m_data.at(index);
  }

  void EventsContainer::Clear()
  {
    if (m_data.empty())
    {
      return;
    }

    m_data.clear();
    this->NotifyDataChanged();
  }

  size_t EventsContainer::Size()
  {
    return m_data.size();
  }
}
// db namespace