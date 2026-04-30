/**
 * @file EventsContainer.hpp
 * @brief Container class for managing collections of LogEvent objects.
 * @author LogViewer Development Team
 * @date 2025
 */

#pragma once
#include "LogEvent.hpp"
#include "IModel.hpp"
#include <atomic>
#include <shared_mutex>
#include <vector>

/**
 * @namespace db
 * @brief Database and data model components for the LogViewer application.
 */
namespace db
{

/**
 * @class EventsContainer
 * @brief High-performance, thread-safe container for managing large collections of LogEvent
 * objects.
 *
 * EventsContainer provides efficient storage and access to LogEvent
 * collections, optimized for handling very large log files (potentially
 * millions of events). The container supports random access, iteration, and
 * provides methods for virtual list control integration.
 *
 * @par Design Goals
 * - **Performance**: O(1) random access, efficient batch operations
 * - **Thread Safety**: Safe for concurrent reads and exclusive writes
 * - **Integration**: Works as IModel for MVC-based UI rendering
 * - **Scalability**: Tested with millions of events
 *
 * @par Performance Characteristics
 * - Random access: O(1)
 * - Insertion at end: O(1) amortized
 * - Insertion via AddEventBatch: O(k) amortized, k = batch size
 * - Memory usage: Optimized for large datasets with no fragmentation
 * - Merge operation: O(n + m) for n + m total events
 *
 * @par Thread Safety Model
 * **Thread-safe**: All public methods are protected by internal std::shared_mutex locks.
 * - **Multiple Concurrent Readers**: Multiple threads can call GetEvent(), Size(), etc.
 *   simultaneously (shared_lock allows concurrent access)
 * - **Exclusive Writers**: Write operations (AddEvent, AddEventBatch, Clear, MergeEvents)
 *   use unique_lock to ensure only one writer at a time
 * - **Safe Parser Integration**: Parser threads can add events while UI thread reads
 * - **Lock Strategy Implementation**:
 *   - Read operations: `std::shared_lock<std::shared_mutex> lock(m_eventMutex);`
 *   - Write operations: `std::unique_lock<std::shared_mutex> lock(m_eventMutex);`
 *
 * @par Usage Examples
 *
 * **Adding individual events:**
 * @code
 * db::EventsContainer container;
 *
 * // Create and add event (moved into container)
 * db::LogEvent event(1, {
 *     {"timestamp", "2025-03-15T12:00:00Z"},
 *     {"level", "INFO"},
 *     {"message", "Application started"}
 * });
 * container.AddEvent(std::move(event));
 *
 * // Access the stored event
 * const auto& stored = container.GetEvent(0);
 * std::cout << stored.findByKey("message") << std::endl;
 * @endcode
 *
 * **Batch operations for efficiency:**
 * @code
 * std::vector<std::pair<int, db::LogEvent::EventItems>> batch;
 * for (int i = 0; i < 1000; ++i) {
 *     batch.push_back({i, {
 *         {"timestamp", "2025-03-15"},
 *         {"sequence", std::to_string(i)}
 *     }});
 * }
 * container.AddEventBatch(std::move(batch));  // Efficient bulk insertion
 * std::cout << "Container size: " << container.Size() << std::endl;
 * @endcode
 *
 * **Thread-safe concurrent access:**
 * @code
 * // Parser thread
 * std::thread parser([&container](const std::string& filename) {
 *     // Parse and add events...
 *     for (const auto& event_data : parsed_events) {
 *         container.AddEvent(db::LogEvent(...));  // Thread-safe
 *     }
 * });
 *
 * // UI thread (concurrent read)
 * std::thread ui([&container]() {
 *     for (size_t i = 0; i < container.Size(); ++i) {
 *         const auto& event = container.GetEvent(i);  // Thread-safe read
 *         // Render event in UI...
 *     }
 * });
 * @endcode
 *
 * **Merging events from multiple sources:**
 * @code
 * db::EventsContainer log_file_1;
 * db::EventsContainer log_file_2;
 *
 * // Parse into separate containers...
 * // Merge events by timestamp, preserving source information
 * log_file_1.MergeEvents(log_file_2, "app.log", "system.log", "timestamp");
 *
 * // Events now interleaved by timestamp, with source identifiers preserved
 * @endcode
 *
 * @see LogEvent for individual event representation
 * @see mvc::IModel for model interface details
 * @see FilterManager for filtering merged events
 */
class EventsContainer : public mvc::IModel
{
  public:
    /**
     * @brief Default constructor.
     *
     * Creates an empty container ready to accept LogEvent objects.
     */
    EventsContainer();
    /**
     * @brief Destructor.
     *
     * Cleans up resources and clears the internal event storage.
     */
    virtual ~EventsContainer();

    /**
     * @brief Adds a new event to the container.
     *
     * @param event The LogEvent to add (moved into the container)
    * @note The event is moved to avoid unnecessary copying
    * @par Complexity
    * O(1) amortized
     */
    void AddEvent(LogEvent&& event);

    /**
     * @brief Adds a batch of new events to the container.
     *
     * @param eventBatch A vector of pairs, each containing an integer and
     *        a LogEvent::EventItems object, representing the batch of events
     *        to add (moved into the container)
    * @note The eventBatch is moved to avoid unnecessary copying
    * @par Complexity
    * O(k) amortized, where k is the number of events in the batch
     */
    void AddEventBatch(
        std::vector<std::pair<int, LogEvent::EventItems>>&& eventBatch);

    /**
     * @brief Retrieves an event by index.
     *
     * @param index The zero-based index of the event to retrieve
    * @return const LogEvent& Reference to the event at the specified index
    * @throws std::out_of_range if index is >= Size()
    * @par Complexity
    * O(1)
     */
    const LogEvent& GetEvent(const int index);

  public: // IModel interface
    /** @brief Adds a new item to the model.
     * @param item The LogEvent to add (moved into the model)
     * @note This method is used to add new events to the model.
     */
    void AddItem(LogEvent&& item) override;

    /**
     * @brief Gets the number of events in the container.
     *
    * @return size_t The number of stored events
    * @par Complexity
    * O(1)
     */
    size_t Size() const override;

    /**
     * @brief Sets the currently selected item index.
     *
     * Updates the internal current item index for virtual list control
     * synchronization.
     *
     * @param index The index of the item to set as current
     * @note This method is used for UI synchronization purposes
     */
    void SetCurrentItem(const int index) override;

    /**
     * @brief Gets the currently selected item index.
     *
     * @return int The index of the currently selected item, or -1 if none
     * selected
     */
    int GetCurrentItemIndex() override;

    /**
     * @brief Removes all events from the container.
     *
    * After this call, Size() returns 0 and the container is ready for new
    * events.
    * @par Complexity
    * O(n) where n is the number of stored events
     */
    void Clear() override;
    
    /**
      * @brief Merges events from another container by timestamp (stable).
      *
      * Performs a stable merge of two *already ordered* event sequences using the
      * provided timestamp field.
      *
      * Key property: events with the same timestamp keep their original order
      * within each source container (no reordering of existing events).
      *
      * Events missing the timestamp field are treated as having an empty
      * timestamp and are appended after all events with valid timestamps (while
      * still preserving their internal order).
     * 
     * @param other Container with events to merge
     * @param existingAlias Identifier for events already in this container
     * @param newAlias Identifier for events from the 'other' container
     * @param timestampField Name of the field containing timestamp (e.g., "timestamp", "time")
     * @note All existing events will have their source set to existingAlias
     * @note All events from 'other' will have their source set to newAlias
     */
    void MergeEvents(EventsContainer& other, 
                     const std::string& existingAlias,
                     const std::string& newAlias,
                     const std::string& timestampField = "timestamp");

    /**
     * @brief Gets an event by index.
     *
     * @param index The zero-based index of the event to retrieve
     * @return LogEvent& Reference to the event at the specified index
     * @throws std::out_of_range if index is >= Size()
     */
    db::LogEvent& GetItem(const int index) override;
    const db::LogEvent& GetItem(const int index) const override;

    /**
     * @brief Temporarily disables view notifications during bulk loading.
     *
     * Call before a background-thread bulk insert to prevent Qt widget
     * callbacks from being invoked on the wrong thread. Resume with
     * ResumeNotifications() and then refresh the view manually.
     */
    void SuspendNotifications() noexcept
    {
        m_notificationsEnabled.store(false, std::memory_order_release);
    }

    /**
     * @brief Re-enables view notifications after bulk loading.
     */
    void ResumeNotifications() noexcept
    {
        m_notificationsEnabled.store(true, std::memory_order_release);
    }

  private:
    std::vector<LogEvent> m_data; ///< Internal storage for events
    /// Currently selected item index (-1 = no selection).
    /// Declared atomic so that GetCurrentItemIndex() / SetCurrentItem()
    /// are safe without acquiring m_mutex.
    std::atomic<int> m_currentItem {-1};
    mutable std::shared_mutex m_mutex; ///< Reader-writer lock for thread safety
    /// When false, AddEvent/AddEventBatch skip NotifyDataChanged so that
    /// Qt widget calls are not made from a background parser thread.
    std::atomic<bool> m_notificationsEnabled{true};
};

} // namespace db
