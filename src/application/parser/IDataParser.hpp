#pragma once
#include "db/LogEvent.hpp"
#include "spdlog/spdlog.h"
#include <functional>
#include <vector>

namespace parser
{

class IDataParserObserver
{
  public:
    virtual ~IDataParserObserver() = default;
    virtual void NewEventFound(db::LogEvent&& event) = 0;
    virtual void ProgressUpdated() = 0;
};

class IDataParser
{
  public:
    virtual ~IDataParser() = default;

    virtual void ParseData(const std::string& filepath) = 0;
    virtual void ParseData(std::istream& input) = 0;
    virtual uint32_t GetCurrentProgress() const = 0;
    virtual uint32_t GetTotalProgress() const = 0;

    void AddObserver(IDataParserObserver* observer)
    {
        if (observer)
        {
            observers.push_back(observer);
        }
    }

    void RegisterObserver(IDataParserObserver* observer)
    {
        AddObserver(observer);
    }

    void RemoveObserver(IDataParserObserver* observer)
    {
        if (observer)
        {
            auto it = std::find(observers.begin(), observers.end(), observer);
            if (it != observers.end())
            {
                observers.erase(it);
            }
        }
    }

    void UnregisterObserver(IDataParserObserver* observer)
    {
        RemoveObserver(observer);
    }

    void NotifyNewEvent(db::LogEvent&& event)
    {
        if (observers.empty())
        {
            return;
        }

        // Handle all observers except the last one
        for (size_t i = 0; i < observers.size() - 1; ++i)
        {
            if (observers[i])
            {
                db::LogEvent eventCopy = event; // Make a copy
                observers[i]->NewEventFound(std::move(eventCopy));
            }
        }

        // Handle the last observer (move the original event)
        if (!observers.empty() && observers.back())
        {
            observers.back()->NewEventFound(std::move(event));
        }
    }

    void NotifyProgressUpdated()
    {
        for (auto observer : observers)
        {
            observer->ProgressUpdated();
        }
    }

  private:
    std::vector<IDataParserObserver*> observers;
};

} // namespace parser
