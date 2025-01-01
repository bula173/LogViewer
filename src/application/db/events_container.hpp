#ifndef DB_EVENTSCONTAINER_HPP
#define DB_EVENTSCONTAINER_HPP

#include <vector>
#include <ranges>

#include "db/event.hpp"
#include "mvc/model.hpp"

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
