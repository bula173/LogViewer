/**
 * @file EventCommands.hpp
 * @brief Concrete commands for event operations
 * @author LogViewer Development Team
 * @date 2025
 *
 * Implements specific commands for manipulating event data:
 * - ClearEventsCommand: Clear all events with undo
 * - AddEventsCommand: Batch add events with undo
 */

#pragma once

#include "command/ICommand.hpp"
#include "db/EventsContainer.hpp"
#include "db/LogEvent.hpp"
#include <vector>

namespace command {

/**
 * @class ClearEventsCommand
 * @brief Command to clear all events from container
 *
 * Stores events before clearing to support undo.
 */
class ClearEventsCommand : public ICommand {
public:
    explicit ClearEventsCommand(db::EventsContainer& container);

    void execute() override;
    void undo() override;
    std::string getDescription() const override { return "Clear Events"; }

private:
    db::EventsContainer& m_container;
    std::vector<std::pair<int, db::LogEvent::EventItems>> m_savedEvents;
    bool m_executed = false;
};

/**
 * @class AddEventsBatchCommand
 * @brief Command to add multiple events at once
 *
 * Supports undo by tracking how many events were added.
 */
class AddEventsBatchCommand : public ICommand {
public:
    AddEventsBatchCommand(db::EventsContainer& container,
                         std::vector<std::pair<int, db::LogEvent::EventItems>> events);

    void execute() override;
    void undo() override;
    std::string getDescription() const override;

private:
    db::EventsContainer& m_container;
    std::vector<std::pair<int, db::LogEvent::EventItems>> m_events;
    size_t m_originalSize = 0;
    bool m_executed = false;
};

} // namespace command
