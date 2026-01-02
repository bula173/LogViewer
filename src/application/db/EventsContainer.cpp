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
    util::Logger::Trace("EventsContainer::AddEvent called");
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        this->AddItem(std::move(event));
    } // Release lock before notifying views
    
    // Notify views after releasing the lock to avoid deadlock
    this->NotifyDataChanged();
}

void EventsContainer::AddEventBatch(
    std::vector<std::pair<int, LogEvent::EventItems>>&& eventBatch)
{
    util::Logger::Trace("EventsContainer::AddEventBatch called with size: {}",
        eventBatch.size());
    
    // Thread-safe: Use exclusive lock for batch modification
    {
        std::unique_lock<std::shared_mutex> lock(m_mutex);
        m_data.reserve(m_data.size() + eventBatch.size());
        
        for (auto& item : eventBatch)
        {
            m_data.emplace_back(item.first, std::move(item.second));
        }
    } // Release lock before notifying views
    
    // Notify views after releasing the lock to avoid deadlock
    this->NotifyDataChanged();
}

const LogEvent& EventsContainer::GetEvent(const int index)
{
    /**
     * @brief Provides bounds-checked access to events (thread-safe read).
     *
     * Uses vector's at() method which throws std::out_of_range for invalid
     * indices. This ensures safe access even with invalid indices from UI
     * components.
     * Thread-safe: Uses shared lock for concurrent reads.
     */
    util::Logger::Trace("EventsContainer::GetEvent called with index: {}", index);
    std::shared_lock<std::shared_mutex> lock(m_mutex);
    return this->GetItem(index);
}

int EventsContainer::GetCurrentItemIndex()
{
    util::Logger::Trace("EventsContainer::GetCurrentItemIndex called, returning {}",
        m_currentItem);
    return m_currentItem;
}

void EventsContainer::SetCurrentItem(const int item)
{
    /**
     * @brief Updates the current selection index.
     *
     * This method is typically called by virtual list controls to maintain
     * synchronization between the UI selection and the data model.
     */
    util::Logger::Trace("EventsContainer::SetCurrentItem called with item: {}", item);
    m_currentItem = item;
    for (auto v : m_views)
    {
        util::Logger::Trace("EventsContainer::SetCurrentItem notifying view of "
                      "current index update: {}",
            item);
        v->OnCurrentIndexUpdated(item);
    }
}

void EventsContainer::AddItem(LogEvent&& item)
{
    util::Logger::Trace("EventsContainer::AddItem called");
    // Note: AddItem is called from AddEvent which already holds the lock
    // So we don't acquire a lock here to avoid recursive locking
    m_data.push_back(std::forward<decltype(item)>(item));
    // Note: NotifyDataChanged should be called by AddEvent after releasing the lock
}

LogEvent& EventsContainer::GetItem(const int index)
{
    return const_cast<LogEvent&>(std::as_const(*this).GetItem(index));
}

const LogEvent& EventsContainer::GetItem(const int index) const
{
    util::Logger::Trace("EventsContainer::GetItem const called with index: {}", index);
    if (index < 0 || index >= static_cast<int>(m_data.size()))
    {
        util::Logger::Error("EventsContainer::GetItem const: index out of range");
        throw std::out_of_range("Index out of range");
    }
    return m_data.at(static_cast<size_t>(index));
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
    
    // Notify current item update after releasing the lock to avoid deadlock
    for (auto v : m_views)
    {
        util::Logger::Trace("EventsContainer::Clear notifying view of current index update: -1");
        v->OnCurrentIndexUpdated(-1);
    }
    
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
    util::Logger::Trace("EventsContainer::Size called, returning {}", m_data.size());
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

    // Lock both containers exclusively for the duration of the merge to:
    // - avoid deadlocks (consistent lock order via scoped_lock)
    // - ensure we merge a consistent snapshot
    // - prevent any reordering of existing data while we inject
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

        // ISO-8601-like timestamps compare correctly as strings.
        return timestampA < timestampB;
    };

    // Stable merge: preserves relative order within each input sequence.
    // Tie-breaker: existing events are emitted before new events for the same timestamp.
    std::vector<LogEvent> mergedEvents;
    mergedEvents.reserve(m_data.size() + other.m_data.size());

    std::size_t i = 0;
    std::size_t j = 0;

    while (i < m_data.size() && j < other.m_data.size())
    {
        const auto& a = m_data[i];
        const auto& b = other.m_data[j];

        // If b is strictly before a, take b. Otherwise take a.
        // This makes ties (equal timestamps, or both empty) favor existing (a).
        if (isBefore(b, a))
        {
            mergedEvents.push_back(b);
            ++j;
        }
        else
        {
            mergedEvents.push_back(a);
            ++i;
        }
    }

    // Append the remainder in original order
    for (; i < m_data.size(); ++i)
        mergedEvents.push_back(m_data[i]);
    for (; j < other.m_data.size(); ++j)
        mergedEvents.push_back(other.m_data[j]);

    util::Logger::Info("EventsContainer::MergeEvents: Merged {} events, total count: {}",
                       other.m_data.size(), mergedEvents.size());

    m_data = std::move(mergedEvents);
    m_currentItem = -1; // Reset selection after merge
    
    // Notify views
    for (auto v : m_views)
    {
        util::Logger::Trace("EventsContainer::MergeEvents notifying view of current index update: -1");
        v->OnCurrentIndexUpdated(-1);
    }
    
    this->NotifyDataChanged();
}
} // db namespace