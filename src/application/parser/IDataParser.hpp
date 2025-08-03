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
    virtual void NewEventBatchFound(
        std::vector<std::pair<int, db::LogEvent::EventItems>>&& eventBatch) = 0;
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

    /**
     * @brief Registers an observer to receive notifications.
     * @param observer A pointer to the observer object.
     */
    void RegisterObserver(IDataParserObserver* observer)
    {
        if (observer)
        {
            // Prevent adding the same observer multiple times
            auto it = std::find(observers.begin(), observers.end(), observer);
            if (it == observers.end())
            {
                observers.push_back(observer);
            }
        }
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

    void NotifyNewEventBatch(
        std::vector<std::pair<int, db::LogEvent::EventItems>>&& eventBatch)
    {
        if (observers.empty())
        {
            return;
        }

        // Handle all observers except the last one by sending a copy of the
        // batch
        for (size_t i = 0; i < observers.size() - 1; ++i)
        {
            if (observers[i])
            {
                auto eventBatchCopy =
                    eventBatch; // Make a copy of the entire vector
                observers[i]->NewEventBatchFound(std::move(eventBatchCopy));
            }
        }

        // Handle the last observer by moving the original batch to avoid a copy
        if (!observers.empty() && observers.back())
        {
            observers.back()->NewEventBatchFound(std::move(eventBatch));
        }
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
