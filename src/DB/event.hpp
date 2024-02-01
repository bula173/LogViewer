#ifndef DB_EVENT_HPP
#define DB_EVENT_HPP

#include <string>
#include <vector>

namespace db
{

	class Event
	{
	public:
		//(eventFieldName,data)
		using EventItems = std::vector<std::pair<std::string, std::string>>;

		Event(const int id, EventItems &&eventItems);
		int getId() const;
		const EventItems &getEventItems() const;
		const std::string find(const std::string& key) const;

	private:
		int m_id;
		EventItems m_events;
	};

} // namespace db

#endif // DB_EVENT_HPP
