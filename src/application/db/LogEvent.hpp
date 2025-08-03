/**
 * @file LogEvent.hpp
 * @brief Defines the LogEvent class for representing structured log entries.
 * @author LogViewer Development Team
 * @date 2025
 */

#ifndef DB_EVENT_HPP
#define DB_EVENT_HPP

#include <string>
#include <vector>

/**
 * @namespace db
 * @brief Database and data model components for the LogViewer application.
 */
namespace db
{

/**
 * @class LogEvent
 * @brief Represents a single, structured log event with key-value data.
 *
 * A LogEvent encapsulates a log entry as a collection of key-value pairs,
 * providing efficient access to structured log data. Each event has a unique
 * identifier and contains metadata such as timestamps, severity levels, and
 * custom attributes.
 *
 * @par Thread Safety
 * This class is not thread-safe. External synchronization is required when
 * accessing LogEvent instances from multiple threads.
 *
 * @par Memory Management
 * LogEvent uses move semantics and perfect forwarding where possible to
 * minimize unnecessary copying of large data sets.
 */
class LogEvent
{
  public:
    /** @brief A collection of key-value pairs representing event data
     * (eventFieldName, data). */
    using EventItems = std::vector<std::pair<std::string, std::string>>;

    /** @brief Iterator type for traversing event items. */
    using EventItemsIterator =
        std::vector<std::pair<std::string, std::string>>::iterator;

    /** @brief Const iterator type for traversing event items. */
    using EventItemsConstIterator =
        std::vector<std::pair<std::string, std::string>>::const_iterator;

    /**
     * @brief Template constructor with perfect forwarding.
     *
     * Constructs a LogEvent with an ID and forwards arguments to EventItems
     * constructor. This enables efficient construction from various argument
     * types.
     *
     * @tparam Args Variadic template arguments for EventItems construction
     * @param id The unique identifier for the event
     * @param args Arguments forwarded to EventItems constructor
     * @note Uses SFINAE to ensure Args can construct an EventItems object
     */
    template <typename... Args,
        typename =
            std::enable_if_t<std::is_constructible_v<EventItems, Args&&...>>>
    LogEvent(int id, Args&&... args)
        : m_id(id)
        , m_eventItems(std::forward<Args>(args)...)
    {
    }

    /**
     * @brief Constructs a LogEvent from an initializer list.
     *
     * Convenient constructor for creating events with compile-time known data.
     *
     * @param id The unique identifier for the event
     * @param items Initializer list of key-value pairs
     *
     * @par Example
     * @code
     * LogEvent event(1, {{"timestamp", "2025-01-01"}, {"type", "INFO"}});
     * @endcode
     */
    LogEvent(int id,
        std::initializer_list<std::pair<std::string, std::string>> items)
        : m_id(id)
        , m_eventItems(items)
    {
    }

    /**
     * @brief Gets the unique ID of the event.
     *
     * @return int The event's unique identifier
     * @note Event IDs should be unique within a single log file or session
     */
    int getId() const;

    /**
     * @brief Gets a const reference to the event's data items.
     *
     * @return const EventItems& Reference to the internal items collection
     * @note Use this method for read-only access to avoid unnecessary copying
     */
    const EventItems& getEventItems() const;

    /**
     * @brief Finds a value by its key in the event's items.
     *
     * Performs a linear search through the event's key-value pairs to find
     * the first occurrence of the specified key.
     *
     * @param key The key to search for
     * @return const std::string The corresponding value, or empty string if not
     * found
     * @note Returns the first matching value if duplicate keys exist
     * @complexity O(n) where n is the number of items in the event
     */
    const std::string findByKey(const std::string& key) const;

    /**
     * @brief Finds an iterator to the first item matching the search term.
     *
     * Searches through both keys and values for the specified search term.
     *
     * @param search The term to search for in keys and values
     * @return EventItemsIterator Iterator to the found item, or end() if not
     * found
     * @note This method performs a substring search, not exact matching
     */
    EventItemsIterator findInEvent(const std::string& search);

    /**
     * @brief Equality comparison operator.
     *
     * Two LogEvent objects are considered equal if they have the same ID.
     * The actual event data is not compared.
     *
     * @param other The LogEvent to compare against
     * @return bool true if both events have the same ID
     * @note This comparison is based solely on ID, not event content
     */
    bool operator==(const LogEvent& other) const
    {
        return m_id == other.getId();
    }

  private:
    int m_id;                ///< Unique event identifier
    EventItems m_eventItems; ///< The structured data of the event
};

} // namespace db

#endif // DB_EVENT_HPP
