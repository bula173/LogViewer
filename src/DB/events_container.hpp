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
		EventsContainer() {}

		void AddEvent(Event &&event)
		{
			this->AddItem(event);
		}

		const Event &GetEvent(const int index)
		{
			return this->GetItem(index);
		}
	};

} // namespace db

#endif // DB_EVENTSCONTAINER_HPP
