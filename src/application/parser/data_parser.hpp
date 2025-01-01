#ifndef PARSER_DATAPARSER_HPP
#define PARSER_DATAPARSER_HPP

#include <istream>
#include <vector>

#include "db/event.hpp"

namespace parser
{

	class DataParserObserver
	{
	public:
		virtual void ProgressUpdated() const = 0;
		virtual void NewEventFound(db::Event &&event) = 0;
	};

	class DataParser
	{
	public:
		virtual void ParseData(std::istream &input) = 0;
		virtual uint32_t GetCurrentProgress() const = 0;
		virtual uint32_t GetTotalProgress() const = 0;
		virtual db::Event &GetEvent() const = 0;

		void NewEventNotification(db::Event &&event)
		{
			for (auto o : observers)
			{
				o->NewEventFound(std::move(event));
			}
		}

		void SendProgress()
		{
			for (auto o : observers)
			{
				o->ProgressUpdated();
			}
		}
		void RegisterObserver(DataParserObserver *observer)
		{
			observers.push_back(observer);
		}

	private:
		std::vector<DataParserObserver *> observers;
	};

} // namespace parser

#endif // PARSER_DATAPARSER_HPP
