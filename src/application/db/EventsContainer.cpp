/**
 * @file EventsContainer.cpp
 * @brief Implementation of the EventsContainer class methods.
 * @author LogViewer Development Team
 * @date 2025
 */

#include "EventsContainer.hpp"
#include "Logger.hpp"

#include <utility>
#include <vector>


namespace db
{
EventsContainer::EventsContainer()
{
    util::Logger::Debug("EventsContainer::EventsContainer constructed");
}

EventsContainer::~EventsContainer()
{
    util::Logger::Debug("EventsContainer::~EventsContainer destructed");
}

void EventsContainer::AddEvent(LogEvent&& event)
{
    /**
     * @brief Moves the event into the internal vector storage (thread-safe).
     *
     * Uses move semantics to avoid copying large event data structures.
     * The vector will automatically manage memory allocation and resizing.
     * Thread-safe: Uses exclusive lock for modification.
     */
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        this->AddItem(std::move(event));
    } // Release lock before notifying views

    // Skip notifications during bulk loading (e.g., background thread parsing)
    if (m_notificationsEnabled.load(std::memory_order_acquire))
        this->NotifyDataChanged();
}

void EventsContainer::AddEventBatch(
    std::vector<std::pair<int, LogEvent::EventItems>>&& eventBatch)
{
    // Thread-safe: Use exclusive lock for batch modification
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_data.reserve(m_data.size() + eventBatch.size());

        for (auto& item : eventBatch)
        {
            m_data.emplace_back(item.first, std::move(item.second));
        }
    } // Release lock before notifying views

    // Skip notifications during bulk loading (e.g., background thread parsing)
    if (m_notificationsEnabled.load(std::memory_order_acquire))
        this->NotifyDataChanged();
}

const LogEvent& EventsContainer::GetEvent(size_t index)
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return this->GetItem(index);
}

int EventsContainer::GetCurrentItemIndex()
{
    return m_currentItem.load(std::memory_order_relaxed);
}

void EventsContainer::SetCurrentItem(const int item)
{
    /**
     * @brief Updates the current selection index.
     *
     * This method is typically called by virtual list controls to maintain
     * synchronization between the UI selection and the data model.
     */
    m_currentItem.store(item, std::memory_order_relaxed);

    // Notify via new IModelObservable pattern (weak_ptr observers)
    this->NotifyCurrentIndexUpdated(item);

    // Backward compatibility: also notify old IModel raw-pointer observers
    for (auto v : m_views)
        v->OnCurrentIndexUpdated(item);
}

void EventsContainer::AddItem(LogEvent&& item)
{
    // Note: AddItem is called from AddEvent which already holds the lock
    // So we don't acquire a lock here to avoid recursive locking
    m_data.push_back(std::forward<decltype(item)>(item));
    // Note: NotifyDataChanged should be called by AddEvent after releasing the lock
}

LogEvent& EventsContainer::GetItem(size_t index)
{
    return const_cast<LogEvent&>(std::as_const(*this).GetItem(index));
}

const LogEvent& EventsContainer::GetItem(size_t index) const
{
    if (index >= m_data.size())
        throw std::out_of_range("EventsContainer::GetItem: index out of range");
    return m_data[index];
}

void EventsContainer::Clear()
{
    /**
     * @brief Clears all stored events and resets selection (thread-safe).
     *
     * Removes all events from the container and resets the current item
     * selection to indicate no selection. This method is typically called when
     * loading a new file or clearing the current session.
     * Thread-safe: Uses exclusive lock for modification.
     */
    util::Logger::Debug("EventsContainer::Clear called");
    
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        if (m_data.empty())
        {
            util::Logger::Debug("EventsContainer::Clear: m_data already empty");
            return;
        }

        m_data.clear();
        m_currentItem = -1; // Reset to no selection since container is empty
    } // Release lock before notifying views

    // Notify current item update via new IModelObservable pattern (weak_ptr observers)
    this->NotifyCurrentIndexUpdated(-1);

    // Backward compatibility: also notify old IModel raw-pointer observers
    for (auto v : m_views)
        v->OnCurrentIndexUpdated(-1);

    // Notify views of data change after releasing the lock to avoid deadlock
    this->NotifyDataChanged();
}

size_t EventsContainer::Size() const
{
    /**
     * @brief Returns the current number of stored events (thread-safe).
     *
     * Delegates to the underlying vector's size() method for O(1) performance.
     * Thread-safe: Uses shared lock for concurrent reads.
     */
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return m_data.size();
}

void EventsContainer::MergeEvents(EventsContainer& other, 
                                  const std::string& existingAlias,
                                  const std::string& newAlias,
                                  const std::string& timestampField)
{
    /**
     * @brief Merges events from another container, sorted by timestamp.
     *
     * This method combines events from both containers and sorts them by the
     * specified timestamp field. Events from this container get existingAlias,
     * and events from the 'other' container get newAlias as their source.
     * Events without a valid timestamp will be appended at the end.
     *
     * @param other The container to merge from
     * @param existingAlias The source identifier for existing events
     * @param newAlias The source identifier for new events
     * @param timestampField The field name containing the timestamp
     *
     * Thread-safe: Uses exclusive lock for modification.
     */
    util::Logger::Debug("EventsContainer::MergeEvents called with existingAlias='{}', newAlias='{}', timestampField='{}'", 
                        existingAlias, newAlias, timestampField);

    if (&other == this)
    {
        util::Logger::Warn("EventsContainer::MergeEvents: Attempted to merge container with itself; ignoring.");
        return;
    }

    // We need to lock both containers while we inspect and merge their
    // internal vectors. However, notifying views while holding those locks
    // may cause deadlocks if view callbacks call back into this container.
    // Acquire the locks only for the merge and release them before
    // invoking any callbacks.

    std::vector<LogEvent> mergedEvents;
    mergedEvents.reserve(m_data.size() + other.m_data.size());

    {
        std::scoped_lock lock(m_mutex, other.m_mutex);

        // Set source on existing events (only if not set)
        for (auto& event : m_data)
        {
            if (event.GetSource().empty())
                event.SetSource(existingAlias);
        }

        // Set source on events being merged (always set to newAlias)
        for (auto& event : other.m_data)
            event.SetSource(newAlias);

        auto isBefore = [&timestampField](const LogEvent& a, const LogEvent& b) -> bool {
            const std::string timestampA = a.findByKey(timestampField);
            const std::string timestampB = b.findByKey(timestampField);

            // Empty timestamps are treated as "after" any non-empty timestamp
            if (timestampA.empty() && timestampB.empty())
                return false;
            if (timestampA.empty())
                return false;
            if (timestampB.empty())
                return true;

            // Compare timestamps first
            if (timestampA != timestampB)
                return timestampA < timestampB;

            // If timestamps are equal, use event ID as tiebreaker
            return a.getId() < b.getId();
        };

        // Stable merge: preserves relative order within each input sequence.
        // Tie-breaker: existing events are emitted before new events for the same timestamp.
        auto it1 = m_data.begin();
        auto it2 = other.m_data.begin();

        while (it1 != m_data.end() && it2 != other.m_data.end())
        {
            // If it2 (new event) is strictly before it1 (existing event), take it2.
            // Otherwise take it1. This makes ties favor existing events.
            if (isBefore(*it2, *it1))
            {
                mergedEvents.push_back(std::move(*it2));
                ++it2;
            }
            else
            {
                mergedEvents.push_back(std::move(*it1));
                ++it1;
            }
        }

        // Continue merging remaining items from whichever container has items left
        while (it1 != m_data.end() || it2 != other.m_data.end())
        {
            if (it1 == m_data.end())
            {
                // No more items in m_data, append remaining from other
                mergedEvents.push_back(std::move(*it2));
                ++it2;
            }
            else if (it2 == other.m_data.end())
            {
                // No more items in other, append remaining from m_data
                mergedEvents.push_back(std::move(*it1));
                ++it1;
            }
            else
            {
                // Both have items, compare and take the one that comes first
                if (isBefore(*it2, *it1))
                {
                    mergedEvents.push_back(std::move(*it2));
                    ++it2;
                }
                else
                {
                    mergedEvents.push_back(std::move(*it1));
                    ++it1;
                }
            }
        }

        util::Logger::Info("EventsContainer::MergeEvents: Merged {} events, total count: {}",
                           other.m_data.size(), mergedEvents.size());

        // Store original IDs and reassign sequential IDs based on merge order
        for (int i = 0; i < static_cast<int>(mergedEvents.size()); ++i)
        {
            auto& event = mergedEvents[static_cast<size_t>(i)];
            
            // Store the original ID before reassignment
            event.SetOriginalId(event.getId());
            
            // Update ID to sequential index in merged list
            event.SetId(i);
            
            util::Logger::Debug("EventsContainer::MergeEvents: Event reassigned - original_id={}, new_id={}",
                               event.findByKey("original_id"), event.getId());
        }

        // Replace internal data while still holding the locks to avoid races.
        m_data = std::move(mergedEvents);
        m_currentItem = -1; // Reset selection after merge
    }

    // Locks released here. Notify views without holding the container mutexes.

    // Notify current item update via new IModelObservable pattern (weak_ptr observers)
    this->NotifyCurrentIndexUpdated(-1);

    // Backward compatibility: also notify old IModel raw-pointer observers
    for (auto v : m_views)
        v->OnCurrentIndexUpdated(-1);

    this->NotifyDataChanged();
}

void EventsContainer::NotifyDataChanged()
{
    // Notify via new IModelObservable pattern (weak_ptr observers)
    mvc::IModelObservable::NotifyDataChanged();

    // Backward compatibility: also notify old IModel raw-pointer observers
    mvc::IModel::NotifyDataChanged();
}

} // db namespace