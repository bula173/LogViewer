/**
 * @file EventsContainer.hpp
 * @brief Container class for managing collections of LogEvent objects.
 * @author LogViewer Development Team
 * @date 2025
 */

#pragma once
#include "db/LogEvent.hpp"
#include "mvc/IModel.hpp"
#include <vector>

/**
 * @namespace db
 * @brief Database and data model components for the LogViewer application.
 */
namespace db
{

/**
 * @class EventsContainer
 * @brief High-performance container for managing large collections of LogEvent
 * objects.
 *
 * EventsContainer provides efficient storage and access to LogEvent
 * collections, optimized for handling very large log files (potentially
 * millions of events). The container supports random access, iteration, and
 * provides methods for virtual list control integration.
 *
 * @par Performance Characteristics
 * - Random access: O(1)
 * - Insertion at end: O(1) amortized
 * - Memory usage: Optimized for large datasets
 *
 * @par Thread Safety
 * This class is not thread-safe. External synchronization is required when
 * accessing from multiple threads during modification operations.
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
     * @complexity O(1) amortized
     */
    void AddEvent(LogEvent&& event);

    /**
     * @brief Adds a batch of new events to the container.
     *
     * @param eventBatch A vector of pairs, each containing an integer and
     *        a LogEvent::EventItems object, representing the batch of events
     *        to add (moved into the container)
     * @note The eventBatch is moved to avoid unnecessary copying
     * @complexity O(k) amortized, where k is the number of events in the batch
     */
    void AddEventBatch(
        std::vector<std::pair<int, LogEvent::EventItems>>&& eventBatch);

    /**
     * @brief Retrieves an event by index.
     *
     * @param index The zero-based index of the event to retrieve
     * @return const LogEvent& Reference to the event at the specified index
     * @throws std::out_of_range if index is >= Size()
     * @complexity O(1)
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
     * @complexity O(1)
     */
    size_t Size() override;

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
     * @return long The index of the currently selected item, or -1 if none
     * selected
     */
    int GetCurrentItemIndex() override;

    /**
     * @brief Removes all events from the container.
     *
     * After this call, Size() returns 0 and the container is ready for new
     * events.
     * @complexity O(n) where n is the number of stored events
     */
    void Clear() override;

    /**
     * @brief Gets an event by index.
     *
     * @param index The zero-based index of the event to retrieve
     * @return LogEvent& Reference to the event at the specified index
     * @throws std::out_of_range if index is >= Size()
     */
    db::LogEvent& GetItem(const int index) override;

  private:
    std::vector<LogEvent> m_data; ///< Internal storage for events
    long m_currentItem {
        -1}; ///< Currently selected item index (-1 = no selection)
};

} // namespace db
