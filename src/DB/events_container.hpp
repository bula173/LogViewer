#ifndef DB_EVENTSCONTAINER_HPP
#define DB_EVENTSCONTAINER_HPP

#include <vector>
#include <ranges>
#include "event.hpp"
#include "MVC/model.hpp"

namespace db
{

	class EventsContainer : public mvc::Model<std::vector<Event>>
	{

	public:
		EventsContainer() : m_events(Model::m_data) {}

		void AddEvent(Event &&event)
		{
			m_events.push_back(event);
			this->NotifyDataChanged();
		}

		const Event &GetEvent(const int index)
		{
			return m_events.at(index);
		}

		const int Size()
		{
			return m_events.size();
		}

	private:
		std::vector<Event> &m_events;
	};

} // namespace db

#endif // DB_EVENTSCONTAINER_HPP
