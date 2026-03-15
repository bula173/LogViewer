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
#include <string_view>
#include <unordered_map>
#include "Concepts.hpp"

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
 * @par Design Philosophy
 * LogEvent is optimized for:
 * - Efficient key-value lookups via internal hash index
 * - Memory efficiency through move semantics and perfect forwarding
 * - Integration with filtering and display systems
 * - Support for multi-source event merging with original ID tracking
 *
 * @par Key Features
 * - Fast key-value lookup: O(1) amortized via cached LookupIndex
 * - Multiple source tracking: Identifies events from different log files
 * - Original ID preservation: Retains pre-merge IDs for traceability
 * - Flexible construction: Supports initializer lists and perfect forwarding
 *
 * @par Thread Safety
 * This class is not thread-safe. External synchronization is required when
 * accessing LogEvent instances from multiple threads. For thread-safe collection
 * management, use EventsContainer instead.
 *
 * @par Memory Management
 * LogEvent uses move semantics and perfect forwarding where possible to
 * minimize unnecessary copying of large data sets. Events are typically
 * moved into EventsContainer for storage.
 *
 * @par Usage Examples
 *
 * **Creating events from initializer list:**
 * @code
 * db::LogEvent event(42, {
 *     {"timestamp", "2025-03-15T12:00:00Z"},
 *     {"level", "ERROR"},
 *     {"message", "Database connection failed"},
 *     {"source", "db_engine"}
 * });
 *
 * // Accessing data
 * std::string timestamp = event.findByKey("timestamp");
 * std::cout << "Level: " << event.findByKey("level") << std::endl;
 * @endcode
 *
 * **Searching for values:**
 * @code
 * if (!event.findByKey("error_code").empty()) {
 *     std::cout << "Error code: " << event.findByKey("error_code") << std::endl;
 * }
 * @endcode
 *
 * **Iterating through event data:**
 * @code
 * for (const auto& [key, value] : event.getEventItems()) {
 *     std::cout << key << " = " << value << std::endl;
 * }
 * @endcode
 *
 * **Working with merged events:**
 * @code
 * db::LogEvent original_event(1, {{"msg", "warning"}});
 * original_event.SetOriginalId(1);  // Track original ID
 * original_event.SetId(100);        // Assign merged ID
 * original_event.SetSource("log_file_1.txt");  // Track source
 * @endcode
 *
 * @see EventsContainer for thread-safe collection management
 * @see FilterManager for filtering events based on key-value data
 */
class LogEvent
{
  public:
    /** @brief A collection of key-value pairs representing event data
     * (eventFieldName, data). */
    using EventItems = std::vector<std::pair<std::string, std::string>>;

    /** @brief Fast lookup index for key-value pairs */
    using LookupIndex = std::unordered_map<std::string, size_t>;

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
     * types without unnecessary copies.
     *
     * @tparam Args Variadic template arguments for EventItems construction
     * @param id The unique identifier for the event (typically assigned by parser)
     * @param args Arguments forwarded to EventItems constructor
     *
     * @note Uses C++20 concepts to constrain args that can construct an EventItems object
     *
     * @par Usage
     * @code
     * // From vector of pairs
     * std::vector<std::pair<std::string, std::string>> data = {...};
     * db::LogEvent event(123, data);  // or std::move(data)
     *
     * // From initializer list (handled by overload)
     * db::LogEvent event(456, {{"key", "value"}});
     * @endcode
     *
     * @see LogEvent(int, std::initializer_list)
     */
    template <typename... Args>
    requires util::Constructible<EventItems, Args&&...>
    LogEvent(int id, Args&&... args)
        : m_id(id)
        , m_eventItems(std::forward<Args>(args)...)
    {
        buildLookupIndex();
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
        buildLookupIndex();
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
     * Performs a fast lookup using the internal hash index to retrieve the
     * value associated with the first occurrence of a key. If the key is not found,
     * returns an empty string.
     *
     * @param key The key to search for (e.g., "timestamp", "level", "message")
     * @return const std::string The corresponding value, or empty string if not found
     *
     * @note Returns the first matching value if duplicate keys exist in event data
     * @note The lookup is fast due to internal caching (O(1) average case)
     *
     * @par Example
     * @code
     * db::LogEvent event(1, {
     *     {"timestamp", "2025-03-15"},
     *     {"level", "INFO"},
     *     {"message", "User logged in"}
     * });
     *
     * std::string timestamp = event.findByKey("timestamp");  // "2025-03-15"
     * std::string missing = event.findByKey("nonexistent");   // ""
     * @endcode
     *
     * @see findAllByKey() for retrieving all values with the same key
     * @see findInEvent() for substring search in both keys and values
     *
     * @par Complexity
     * O(1) average case (hash table lookup)
     */
    const std::string findByKey(std::string_view key) const;


    /**
     * @brief Finds all values by its key in the event's items.
     *
     * Performs a linear search through the event's key-value pairs to find
     * all occurrences of the specified key.
     *
     * @param key The key to search for
     * @return const std::string The corresponding value, or empty string if not
     * found
    * @note Returns the first matching value if duplicate keys exist
    * @par Complexity
    * O(n) where n is the number of items in the event
     */
    const std::vector<std::string> findAllByKey(std::string_view key) const;


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
    
    /**
     * @brief Sets the original ID before merge (stores as event data)
     *
     * When merging events from different sources, the original event ID is 
     * preserved as a data field so users can see both the original ID and
     * the new merged ID.
     *
     * @param originalId The original event ID before merge
     */
    void SetOriginalId(int originalId);

    /**
     * @brief Updates the event's ID (used during merge to reassign sequential IDs)
     *
     * After merging events, this method reassigns event IDs to be sequential
     * based on their position in the merged list. The original ID is preserved
     * separately via SetOriginalId().
     *
     * @param newId The new event ID to assign
     */
    void SetId(int newId);

    /**
     * @brief Gets the source identifier for this event
     * @return const std::string& The source identifier (empty for legacy events)
     */
    const std::string& GetSource() const { return m_source; }
    
    /**
     * @brief Sets the source identifier for this event
     * @param source The source identifier (e.g., file name or alias)
     */
    void SetSource(const std::string& source) { m_source = source; }

  private:
    /**
     * @brief Builds the fast lookup index from the event items
     */
    void buildLookupIndex();

    int m_id;                ///< Unique event identifier
    EventItems m_eventItems; ///< The structured data of the event
    LookupIndex m_lookupIndex; ///< Fast lookup index for keys
    std::string m_source;    ///< Source identifier for multi-file merge
};

} // namespace db

#endif // DB_EVENT_HPP
