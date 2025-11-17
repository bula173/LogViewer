/**
 * @file EventsContainer.cpp
 * @brief Implementation of the EventsContainer class methods.
 * @author LogViewer Development Team
 * @date 2025
 */

#include "EventsContainer.hpp"
#include "util/Logger.hpp"

#include <utility>


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
    
    std::unique_lock<std::shared_mutex> lock(m_mutex);
    if (m_data.empty())
    {
        util::Logger::Debug("EventsContainer::Clear: m_data already empty");
        return;
    }

    m_data.clear();
    SetCurrentItem(0); // Reset current item index
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
} // db namespace