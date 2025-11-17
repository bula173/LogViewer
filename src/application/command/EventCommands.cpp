/**
 * @file EventCommands.cpp
 * @brief Implementation of event-related commands
 * @author LogViewer Development Team
 * @date 2025
 */

#include "command/EventCommands.hpp"
#include "util/Logger.hpp"

namespace command {

//-----------------------------------------------------------------------------
// ClearEventsCommand Implementation
//-----------------------------------------------------------------------------

ClearEventsCommand::ClearEventsCommand(db::EventsContainer& container)
    : m_container(container)
{
}

void ClearEventsCommand::execute()
{
    if (m_executed) {
        util::Logger::Warn("ClearEventsCommand::execute - Already executed");
        return;
    }

    util::Logger::Info("ClearEventsCommand::execute - Clearing {} events", 
        m_container.Size());

    // Save current events for undo
    m_savedEvents.clear();
    m_savedEvents.reserve(m_container.Size());

    for (size_t i = 0; i < m_container.Size(); ++i) {
        const auto& event = m_container.GetEvent(static_cast<int>(i));
        m_savedEvents.emplace_back(event.getId(), event.getEventItems());
    }

    m_container.Clear();
    m_executed = true;

    util::Logger::Debug("ClearEventsCommand::execute - Saved {} events for undo",
        m_savedEvents.size());
}

void ClearEventsCommand::undo()
{
    if (!m_executed) {
        util::Logger::Warn("ClearEventsCommand::undo - Not executed yet");
        return;
    }

    util::Logger::Info("ClearEventsCommand::undo - Restoring {} events",
        m_savedEvents.size());

    // Restore events (move to satisfy rvalue reference requirement)
    m_container.AddEventBatch(std::move(m_savedEvents));
    m_executed = false;

    util::Logger::Debug("ClearEventsCommand::undo - Restored {} events",
        m_container.Size());
}

//-----------------------------------------------------------------------------
// AddEventsBatchCommand Implementation
//-----------------------------------------------------------------------------

AddEventsBatchCommand::AddEventsBatchCommand(
    db::EventsContainer& container,
    std::vector<std::pair<int, db::LogEvent::EventItems>> events)
    : m_container(container)
    , m_events(std::move(events))
{
}

void AddEventsBatchCommand::execute()
{
    if (m_executed) {
        util::Logger::Warn("AddEventsBatchCommand::execute - Already executed");
        return;
    }

    util::Logger::Info("AddEventsBatchCommand::execute - Adding {} events",
        m_events.size());

    m_originalSize = m_container.Size();
    // Make a copy to preserve m_events for potential re-execution
    auto eventsCopy = m_events;
    m_container.AddEventBatch(std::move(eventsCopy));
    m_executed = true;

    util::Logger::Debug("AddEventsBatchCommand::execute - Container size: {} -> {}",
        m_originalSize, m_container.Size());
}

void AddEventsBatchCommand::undo()
{
    if (!m_executed) {
        util::Logger::Warn("AddEventsBatchCommand::undo - Not executed yet");
        return;
    }

    util::Logger::Info("AddEventsBatchCommand::undo - Removing {} events",
        m_events.size());

    // Remove events by clearing and restoring original size
    // Note: This assumes events were added at the end
    // For more complex scenarios, we'd need to track exact positions

    std::vector<std::pair<int, db::LogEvent::EventItems>> originalEvents;
    originalEvents.reserve(m_originalSize);

    for (size_t i = 0; i < m_originalSize; ++i) {
        const auto& event = m_container.GetEvent(static_cast<int>(i));
        originalEvents.emplace_back(event.getId(), event.getEventItems());
    }

    m_container.Clear();
    m_container.AddEventBatch(std::move(originalEvents));
    m_executed = false;

    util::Logger::Debug("AddEventsBatchCommand::undo - Container size: {} -> {}",
        m_originalSize + m_events.size(), m_container.Size());
}

std::string AddEventsBatchCommand::getDescription() const
{
    return "Add " + std::to_string(m_events.size()) + " Events";
}

} // namespace command
