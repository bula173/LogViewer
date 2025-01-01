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
		using EventItemsIterator = std::vector<std::pair<std::string, std::string>>::iterator;
		using EventItemsConstIterator = std::vector<std::pair<std::string, std::string>>::const_iterator;

		Event(const int id, EventItems &&eventItems);
		int getId() const;
		const EventItems &getEventItems() const;
		const std::string findByKey(const std::string &key) const;
		EventItemsIterator findInEvent(const std::string &search);

		bool operator==(const Event &other) const
		{
			return m_id == other.getId();
		}

	private:
		int m_id;
		EventItems m_events;
	};

} // namespace db

#endif // DB_EVENT_HPP
