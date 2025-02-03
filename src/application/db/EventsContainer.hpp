#ifndef DB_EVENTSCONTAINER_HPP
#define DB_EVENTSCONTAINER_HPP

#include <vector>
#include <ranges>

#include "db/LogEvent.hpp"
#include "mvc/IModel.hpp"

namespace db
{

	class EventsContainer : public mvc::IModel
	{

	public:
		EventsContainer();
		virtual ~EventsContainer();
		void AddEvent(LogEvent &&event);
		const LogEvent &GetEvent(const int index);

	public: // IModel
		int GetCurrentItemIndex() override;
		void SetCurrentItem(const int item) override;
		void AddItem(LogEvent &&item) override;
		LogEvent &GetItem(const int index) override;
		void Clear() override;
		size_t Size() override;

	protected:
		std::vector<LogEvent> m_data;
		int m_currentItem;
	};

} // namespace db

#endif // DB_EVENTSCONTAINER_HPP
