#ifndef DB_EVENT_HPP
#define DB_EVENT_HPP

#include <string>
#include <vector>

namespace db
{

	class LogEvent
	{
	public:
		//(eventFieldName,data)
		using EventItems = std::vector<std::pair<std::string, std::string>>;
		using EventItemsIterator = std::vector<std::pair<std::string, std::string>>::iterator;
		using EventItemsConstIterator = std::vector<std::pair<std::string, std::string>>::const_iterator;

		template <typename... Args, typename = std::enable_if_t<std::is_constructible_v<EventItems, Args &&...>>>
		LogEvent(int id, Args &&...args) : m_id(id), m_eventItems(std::forward<Args>(args)...)
		{
		}

		LogEvent(int id, std::initializer_list<std::pair<std::string, std::string>> items)
				: m_id(id), m_eventItems(items)
		{
		}

		int getId() const;
		const EventItems &getEventItems() const;
		const std::string findByKey(const std::string &key) const;
		EventItemsIterator findInEvent(const std::string &search);

		bool operator==(const LogEvent &other) const
		{
			return m_id == other.getId();
		}

	private:
		int m_id;
		EventItems m_eventItems;
	};

} // namespace db

#endif // DB_EVENT_HPP
