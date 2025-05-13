#ifndef PARSER_IDataParser_HPP
#define PARSER_IDataParser_HPP

#include <istream>
#include <vector>

#include "db/EventsContainer.hpp"
#include "db/LogEvent.hpp"

namespace parser
{

class IDataParserObserver
{
  public:
    virtual void ProgressUpdated() = 0;
    virtual void NewEventFound(db::LogEvent&& event) = 0;
    virtual ~IDataParserObserver() = default;
};

class IDataParser
{
  public:
    virtual void ParseData(std::istream& input) = 0;
    virtual void ParseData(const std::string& filepath) = 0;
    virtual uint32_t GetCurrentProgress() const = 0;
    virtual uint32_t GetTotalProgress() const = 0;
    IDataParser() = default;
    virtual ~IDataParser() = default;
    void NewEventNotification(db::LogEvent&& event)
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
    void RegisterObserver(IDataParserObserver* observer)
    {
        observers.push_back(observer);
    }

  private:
    std::vector<IDataParserObserver*> observers;
};

} // namespace parser

#endif // PARSER_IDataParser_HPP
