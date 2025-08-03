/**
 * @file EventsContainer.cpp
 * @brief Implementation of the EventsContainer class methods.
 * @author LogViewer Development Team
 * @date 2025
 */

#include "EventsContainer.hpp"
#include <spdlog/spdlog.h>

namespace db
{
EventsContainer::EventsContainer()
{
    spdlog::debug("EventsContainer::EventsContainer constructed");
}

EventsContainer::~EventsContainer()
{
    spdlog::debug("EventsContainer::~EventsContainer destructed");
}

void EventsContainer::AddEvent(LogEvent&& event)
{
    /**
     * @brief Moves the event into the internal vector storage.
     *
     * Uses move semantics to avoid copying large event data structures.
     * The vector will automatically manage memory allocation and resizing.
     */
    spdlog::debug("EventsContainer::AddEvent called");
    this->AddItem(std::move(event));
}

void EventsContainer::AddEventBatch(
    std::vector<std::pair<int, LogEvent::EventItems>>&& eventBatch)
{
    spdlog::debug("EventsContainer::AddEventBatch called with size: {}",
        eventBatch.size());
    for (auto& item : eventBatch)
    {
        m_data.emplace_back(item.first, std::move(item.second));
    }
    this->NotifyDataChanged();
}

const LogEvent& EventsContainer::GetEvent(const int index)
{
    /**
     * @brief Provides bounds-checked access to events.
     *
     * Uses vector's at() method which throws std::out_of_range for invalid
     * indices. This ensures safe access even with invalid indices from UI
     * components.
     */
    spdlog::debug("EventsContainer::GetEvent called with index: {}", index);
    return this->GetItem(index);
}

int EventsContainer::GetCurrentItemIndex()
{
    spdlog::debug("EventsContainer::GetCurrentItemIndex called, returning {}",
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
    spdlog::debug("EventsContainer::SetCurrentItem called with item: {}", item);
    m_currentItem = item;
    for (auto v : m_views)
    {
        spdlog::debug("EventsContainer::SetCurrentItem notifying view of "
                      "current index update: {}",
            item);
        v->OnCurrentIndexUpdated(item);
    }
}

void EventsContainer::AddItem(LogEvent&& item)
{
    spdlog::debug("EventsContainer::AddItem called");
    m_data.push_back(std::forward<decltype(item)>(item));
    this->NotifyDataChanged();
}

LogEvent& EventsContainer::GetItem(const int index)
{
    spdlog::debug("EventsContainer::GetItem called with index: {}", index);
    if (index < 0 || index >= static_cast<int>(m_data.size()))
    {
        spdlog::error("EventsContainer::GetItem: index out of range");
        throw std::out_of_range("Index out of range");
    }
    return m_data.at(index);
}

void EventsContainer::Clear()
{
    /**
     * @brief Clears all stored events and resets selection.
     *
     * Removes all events from the container and resets the current item
     * selection to indicate no selection. This method is typically called when
     * loading a new file or clearing the current session.
     */
    spdlog::debug("EventsContainer::Clear called");
    if (m_data.empty())
    {
        spdlog::debug("EventsContainer::Clear: m_data already empty");
        return;
    }

    m_data.clear();
    SetCurrentItem(0); // Reset current item index
    this->NotifyDataChanged();
}

size_t EventsContainer::Size()
{
    /**
     * @brief Returns the current number of stored events.
     *
     * Delegates to the underlying vector's size() method for O(1) performance.
     */
    spdlog::debug("EventsContainer::Size called, returning {}", m_data.size());
    return m_data.size();
}
} // db namespace