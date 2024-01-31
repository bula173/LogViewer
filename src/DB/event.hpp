#ifndef DB_EVENT_HPP
#define DB_EVENT_HPP

#include <string>
#include <unordered_map>

namespace db
{

	class Event
	{
	public:
	  //(eventFieldName,data)
		using EventItems = std::unordered_map<std::string, std::string>;

		Event(const int id, EventItems&& eventItems);
		int getId() const;
		const EventItems& getEventItems() const;

		

	private:
		int m_id;
		EventItems m_events;
	};

} // namespace db

#endif // DB_EVENT_HPP
