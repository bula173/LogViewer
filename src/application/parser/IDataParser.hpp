/**
 * @file IDataParser.hpp
 * @brief Interface definitions for data parsing components.
 * @author LogViewer Development Team
 * @date 2025
 */

#pragma once
#include "../db/LogEvent.hpp"
#include <algorithm>
#include <cstdint>
#include <istream>
#include <vector>

/**
 * @namespace parser
 * @brief Data parsing components and interfaces.
 */
namespace parser
{

// Forward declaration for circular dependency resolution
class IDataParser;

/**
 * @class IDataParserObserver
 * @brief Observer interface for receiving notifications from data parsers.
 *
 * This interface follows the Observer pattern to provide loose coupling between
 * data parsing operations and user interface components. Observers receive
 * notifications about parsing progress and newly discovered events.
 *
 * @par Usage Example
 * @code
 * class MyObserver : public IDataParserObserver {
 * public:
 *     void ProgressUpdated() override {
 *         // Update progress bar in UI
 *     }
 *
 *     void NewEventFound(db::LogEvent&& event) override {
 *         // Add event to display container
 *         events.AddEvent(std::move(event));
 *     }
 * };
 * @endcode
 */
class IDataParserObserver
{
  public:
    /**
     * @brief Virtual destructor for proper cleanup of derived classes.
     */
    virtual ~IDataParserObserver() = default;

    /**
     * @brief Called when parsing progress is updated.
     *
     * This method is invoked periodically during parsing operations to
     * allow observers to update progress indicators in the user interface.
     *
     * @note Implementations should be efficient as this may be called
     * frequently
     */
    virtual void ProgressUpdated() = 0;

    /**
     * @brief Called when a new event is discovered during parsing.
     *
     * @param event The newly parsed LogEvent (moved to avoid copying)
     * @note Implementations should be efficient as this may be called
     *       very frequently during parsing of large files
     */
    virtual void NewEventFound(db::LogEvent&& event) = 0;

    /**
     * @brief Called when a batch of new events is discovered during parsing.
     *
     * For performance reasons, parsers may collect multiple events before
     * notifying observers. This method delivers a batch of events at once.
     *
     * @param eventBatch Vector of (id, items) pairs representing the events
     * @note This method is preferred over NewEventFound for high-throughput
     * scenarios
     */
    virtual void NewEventBatchFound(
        std::vector<std::pair<int, db::LogEvent::EventItems>>&& eventBatch) = 0;
};

/**
 * @class IDataParser
 * @brief Abstract base class for all data parsing implementations.
 *
 * This class defines the common interface for parsing different log file
 * formats. Concrete implementations handle specific formats (XML, JSON, CSV,
 * etc.) while providing a consistent interface for client code.
 *
 * @par Observer Pattern
 * The parser uses the Observer pattern to notify interested parties about
 * parsing progress and discovered events without tight coupling.
 *
 * @par Thread Safety
 * Base class is not thread-safe. Derived classes should document their
 * thread safety guarantees.
 */
class IDataParser
{
  public:
    /**
     * @brief Virtual destructor for proper cleanup of derived classes.
     */
    virtual ~IDataParser() = default;

    /**
     * @brief Parses data from a file path.
     *
     * @param filepath The path to the file to parse
     * @throws std::runtime_error if the file cannot be opened or parsed
     * @note This method should be implemented by derived classes for file-based
     * parsing
     */
    virtual void ParseData(const std::string& filepath) = 0;

    /**
     * @brief Parses data from an input stream.
     *
     * @param input The input stream to read data from
     * @throws std::runtime_error if the stream cannot be read or parsed
     * @note This method enables parsing from various sources (files, network,
     * memory)
     */
    virtual void ParseData(std::istream& input) = 0;

    /**
     * @brief Gets the current parsing progress.
     *
     * @return uint32_t Current progress value (meaning depends on
     * implementation)
     * @note Progress values should be used with GetTotalProgress() for
     * percentage calculations
     */
    virtual uint32_t GetCurrentProgress() const = 0;

    /**
     * @brief Gets the total expected progress value.
     *
     * @return uint32_t Maximum progress value when parsing is complete
     * @note Return 0 if total progress cannot be determined in advance
     */
    virtual uint32_t GetTotalProgress() const = 0;

    /**
     * @brief Registers an observer to receive parsing notifications.
     *
     * @param observer Pointer to the observer object
     * @note The observer must remain valid until parsing completes or it's
     * unregistered
     * @note Duplicate observers are automatically prevented
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

    /**
     * @brief Unregisters a previously registered observer.
     *
     * @param observer Pointer to the observer to remove
     * @note It's safe to call this method with observers that weren't
     * registered
     */
    void UnregisterObserver(IDataParserObserver* observer)
    {
        /**
         * @brief Removes observer from the notification list.
         *
         * Uses std::find to locate the observer and std::vector::erase to
         * remove it. Safe to call with null pointers or unregistered observers.
         */
        auto it = std::find(observers.begin(), observers.end(), observer);
        if (it != observers.end())
        {
            observers.erase(it);
        }
    }

    /**
     * @brief Notifies all observers about a newly found event.
     *
     * @param event The new event to report (moved to avoid copying)
     * @note This method should be called by derived classes when events are
     * discovered
     */
    void NotifyNewEvent(db::LogEvent&& event)
    {
        /**
         * @brief Efficient event distribution to multiple observers.
         *
         * Creates copies for all observers except the last one, then moves
         * the original to the last observer to minimize unnecessary copying.
         */
        for (auto* observer : observers)
        {
            if (observer)
            {
                // Create a copy for all observers except the last one
                if (observer != observers.back())
                {
                    db::LogEvent eventCopy(event.getId(),
                        db::LogEvent::EventItems(event.getEventItems()));
                    observer->NewEventFound(std::move(eventCopy));
                }
                else
                {
                    observer->NewEventFound(std::move(event));
                }
            }
        }
    }

    /**
     * @brief Notifies all observers about parsing progress updates.
     *
     * @note This method should be called periodically by derived classes during
     * parsing
     */
    void NotifyProgressUpdated()
    {
        /**
         * @brief Simple progress notification to all registered observers.
         *
         * Iterates through all observers and calls their ProgressUpdated
         * method. Null pointer checking ensures robustness against invalid
         * observers.
         */
        for (auto* observer : observers)
        {
            if (observer)
            {
                observer->ProgressUpdated();
            }
        }
    }

    /**
     * @brief Notifies all observers about a batch of newly found events.
     *
     * @param eventBatch Vector of (id, items) pairs representing the events
     * @note This is more efficient than calling NotifyNewEvent multiple times
     */
    void NotifyNewEventBatch(
        std::vector<std::pair<int, db::LogEvent::EventItems>>&& eventBatch)
    {
        /**
         * @brief Efficient batch event distribution with copy optimization.
         *
         * Similar to NotifyNewEvent, creates copies for all observers except
         * the last one to minimize memory allocation and copying overhead.
         */
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

  private:
    std::vector<IDataParserObserver*>
        observers; ///< List of registered observers
};

} // namespace parser
