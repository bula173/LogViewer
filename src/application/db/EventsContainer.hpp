#ifndef DB_EVENTSCONTAINER_HPP
#define DB_EVENTSCONTAINER_HPP

#include <vector>
#include <ranges>

#include "db/LogEvent.hpp"
#include "mvc/Model.hpp"

namespace db
{

	class EventsContainer : public mvc::Model<std::vector<LogEvent>>
	{

	public:
		EventsContainer() {}

		void AddEvent(LogEvent &&event)
		{
			this->AddItem(std::move(event));
		}

		const LogEvent &GetEvent(const int index)
		{
			return this->GetItem(index);
		}
	};

} // namespace db

#endif // DB_EVENTSCONTAINER_HPP
