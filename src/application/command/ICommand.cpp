/**
 * @file ICommand.cpp
 * @brief Implementation of command pattern infrastructure
 * @author LogViewer Development Team
 * @date 2025
 */

#include "command/ICommand.hpp"
#include "util/Logger.hpp"
#include "error/Error.hpp"
#include <algorithm>

namespace command {

//-----------------------------------------------------------------------------
// CommandManager Implementation
//-----------------------------------------------------------------------------

CommandManager::CommandManager(size_t maxHistorySize)
    : m_maxHistorySize(maxHistorySize)
{
    util::Logger::Debug("CommandManager created with max history size: {}", maxHistorySize);
}

void CommandManager::execute(std::unique_ptr<ICommand> command)
{
    if (!command) {
        util::Logger::Warn("CommandManager::execute - Null command provided");
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);

    try {
        util::Logger::Info("CommandManager::execute - Executing: {}", 
            command->getDescription());

        command->execute();

        // Only add to undo stack if undoable
        if (command->isUndoable()) {
            m_undoStack.push_back(std::move(command));
            trimHistory();

            // Clear redo stack when new command executed
            m_redoStack.clear();

            util::Logger::Debug("CommandManager::execute - Command added to undo stack (size: {})",
                m_undoStack.size());
        } else {
            util::Logger::Debug("CommandManager::execute - Command not undoable, not added to history");
        }

    } catch (const std::exception& e) {
        util::Logger::Error("CommandManager::execute - Exception executing command '{}': {}",
            command->getDescription(), e.what());
        throw;
    }
}

bool CommandManager::undo()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_undoStack.empty()) {
        util::Logger::Debug("CommandManager::undo - Nothing to undo");
        return false;
    }

    auto command = std::move(m_undoStack.back());
    m_undoStack.pop_back();

    try {
        util::Logger::Info("CommandManager::undo - Undoing: {}", 
            command->getDescription());

        command->undo();
        m_redoStack.push_back(std::move(command));

        util::Logger::Debug("CommandManager::undo - Success (undo: {}, redo: {})",
            m_undoStack.size(), m_redoStack.size());

        return true;

    } catch (const std::exception& e) {
        util::Logger::Error("CommandManager::undo - Exception undoing command '{}': {}",
            command->getDescription(), e.what());
        
        // Put command back in undo stack on failure
        m_undoStack.push_back(std::move(command));
        throw;
    }
}

bool CommandManager::redo()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (m_redoStack.empty()) {
        util::Logger::Debug("CommandManager::redo - Nothing to redo");
        return false;
    }

    auto command = std::move(m_redoStack.back());
    m_redoStack.pop_back();

    try {
        util::Logger::Info("CommandManager::redo - Redoing: {}", 
            command->getDescription());

        command->execute();
        m_undoStack.push_back(std::move(command));

        util::Logger::Debug("CommandManager::redo - Success (undo: {}, redo: {})",
            m_undoStack.size(), m_redoStack.size());

        return true;

    } catch (const std::exception& e) {
        util::Logger::Error("CommandManager::redo - Exception redoing command '{}': {}",
            command->getDescription(), e.what());
        
        // Put command back in redo stack on failure
        m_redoStack.push_back(std::move(command));
        throw;
    }
}

bool CommandManager::canUndo() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_undoStack.empty();
}

bool CommandManager::canRedo() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return !m_redoStack.empty();
}

std::string CommandManager::getUndoDescription() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_undoStack.empty()) {
        return "";
    }
    return m_undoStack.back()->getDescription();
}

std::string CommandManager::getRedoDescription() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_redoStack.empty()) {
        return "";
    }
    return m_redoStack.back()->getDescription();
}

void CommandManager::clear()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    
    util::Logger::Info("CommandManager::clear - Clearing undo/redo stacks (undo: {}, redo: {})",
        m_undoStack.size(), m_redoStack.size());

    m_undoStack.clear();
    m_redoStack.clear();
}

size_t CommandManager::getUndoCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_undoStack.size();
}

size_t CommandManager::getRedoCount() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_redoStack.size();
}

void CommandManager::trimHistory()
{
    // Already holding m_mutex from caller

    if (m_undoStack.size() > m_maxHistorySize) {
        size_t toRemove = m_undoStack.size() - m_maxHistorySize;
        
        util::Logger::Debug("CommandManager::trimHistory - Removing {} old commands",
            toRemove);

        m_undoStack.erase(m_undoStack.begin(), 
                         m_undoStack.begin() + static_cast<std::ptrdiff_t>(toRemove));
    }
}

//-----------------------------------------------------------------------------
// MacroCommand Implementation
//-----------------------------------------------------------------------------

MacroCommand::MacroCommand(std::string description)
    : m_description(std::move(description))
{
}

void MacroCommand::execute()
{
    if (m_executed) {
        util::Logger::Warn("MacroCommand::execute - Already executed, skipping");
        return;
    }

    util::Logger::Info("MacroCommand::execute - Executing macro '{}' ({} commands)",
        m_description, m_commands.size());

    try {
        for (auto& cmd : m_commands) {
            cmd->execute();
        }
        m_executed = true;

    } catch (const std::exception& e) {
        util::Logger::Error("MacroCommand::execute - Exception in macro '{}': {}",
            m_description, e.what());
        
        // Attempt to undo partially executed commands
        util::Logger::Info("MacroCommand::execute - Attempting to undo partial execution");
        try {
            undo();
        } catch (...) {
            util::Logger::Error("MacroCommand::execute - Failed to undo partial execution");
        }
        
        throw;
    }
}

void MacroCommand::undo()
{
    if (!m_executed) {
        util::Logger::Warn("MacroCommand::undo - Not executed yet, nothing to undo");
        return;
    }

    util::Logger::Info("MacroCommand::undo - Undoing macro '{}' ({} commands)",
        m_description, m_commands.size());

    // Undo in reverse order
    for (auto it = m_commands.rbegin(); it != m_commands.rend(); ++it) {
        try {
            (*it)->undo();
        } catch (const std::exception& e) {
            util::Logger::Error("MacroCommand::undo - Exception undoing command in '{}': {}",
                m_description, e.what());
            throw;
        }
    }

    m_executed = false;
}

void MacroCommand::addCommand(std::unique_ptr<ICommand> command)
{
    if (!command) {
        util::Logger::Warn("MacroCommand::addCommand - Null command provided");
        return;
    }

    if (m_executed) {
        util::Logger::Error("MacroCommand::addCommand - Cannot add commands after execution");
        throw error::Error(error::ErrorCode::RuntimeError, "Cannot add commands to executed macro");
    }

    util::Logger::Debug("MacroCommand::addCommand - Adding command: {}", 
        command->getDescription());

    m_commands.push_back(std::move(command));
}

} // namespace command
