/**
 * @file ICommand.hpp
 * @brief Command pattern for undo/redo operations
 * @author LogViewer Development Team
 * @date 2025
 *
 * Implements the Command design pattern for encapsulating operations
 * that can be executed, undone, and redone.
 *
 * @par Example Usage:
 * @code
 * // Clear events with undo support
 * auto clearCmd = std::make_unique<ClearEventsCommand>(eventsContainer);
 * commandManager.execute(std::move(clearCmd));
 * // ... later ...
 * commandManager.undo(); // Restore events
 *
 * // Add filter with undo/redo
 * auto addFilterCmd = std::make_unique<AddFilterCommand>(filterManager, filter);
 * commandManager.execute(std::move(addFilterCmd));
 * @endcode
 *
 * @par Thread Safety:
 * CommandManager uses locks for thread-safe undo/redo operations.
 * Individual commands are not thread-safe by design - they should
 * only be executed by CommandManager.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>
#include <mutex>
#include <deque>

namespace command {

/**
 * @class ICommand
 * @brief Abstract command interface
 *
 * All commands must implement execute() and undo().
 * Commands should be idempotent - executing or undoing multiple times
 * should have the same effect as executing/undoing once.
 */
class ICommand {
public:
    virtual ~ICommand() = default;

    /**
     * @brief Execute the command
     *
     * Perform the operation. This method should be called exactly once
     * by the CommandManager after construction.
     */
    virtual void execute() = 0;

    /**
     * @brief Undo the command
     *
     * Reverse the operation. Should restore the exact state before execute().
     * Must be idempotent - calling multiple times should be safe.
     */
    virtual void undo() = 0;

    /**
     * @brief Get command description for UI
     * @return Human-readable description (e.g., "Clear Events", "Add Filter")
     */
    virtual std::string getDescription() const = 0;

    /**
     * @brief Check if command can be undone
     * @return false for non-reversible operations
     *
     * Some commands like "Save File" or "Send Network Request" may not
     * be undoable. CommandManager will skip these in undo stack.
     */
    virtual bool isUndoable() const { return true; }
};

/**
 * @class CommandManager
 * @brief Manages command execution and undo/redo stacks
 *
 * Maintains undo and redo stacks with configurable limits.
 * Thread-safe for concurrent execute/undo/redo operations.
 *
 * @par Memory Management:
 * Uses bounded deques to prevent unbounded memory growth.
 * Default limit: 50 commands in history.
 */
class CommandManager {
public:
    /**
     * @brief Construct command manager
     * @param maxHistorySize Maximum number of undoable commands (default 50)
     */
    explicit CommandManager(size_t maxHistorySize = 50);

    /**
     * @brief Execute a command and add to undo stack
     * @param command Command to execute (takes ownership)
     *
     * Clears the redo stack when a new command is executed.
     * Thread-safe.
     */
    void execute(std::unique_ptr<ICommand> command);

    /**
     * @brief Undo the last command
     * @return true if undo successful, false if nothing to undo
     *
     * Moves command from undo to redo stack.
     * Thread-safe.
     */
    bool undo();

    /**
     * @brief Redo the last undone command
     * @return true if redo successful, false if nothing to redo
     *
     * Thread-safe.
     */
    bool redo();

    /**
     * @brief Check if undo is available
     * @return true if at least one undoable command in history
     */
    bool canUndo() const;

    /**
     * @brief Check if redo is available
     * @return true if at least one redoable command
     */
    bool canRedo() const;

    /**
     * @brief Get description of next undo command
     * @return Description or empty string if canUndo() is false
     */
    std::string getUndoDescription() const;

    /**
     * @brief Get description of next redo command
     * @return Description or empty string if canRedo() is false
     */
    std::string getRedoDescription() const;

    /**
     * @brief Clear all undo/redo history
     *
     * Useful when switching to a new document or after save.
     * Thread-safe.
     */
    void clear();

    /**
     * @brief Get number of commands in undo stack
     */
    size_t getUndoCount() const;

    /**
     * @brief Get number of commands in redo stack
     */
    size_t getRedoCount() const;

private:
    mutable std::mutex m_mutex;
    std::deque<std::unique_ptr<ICommand>> m_undoStack;
    std::deque<std::unique_ptr<ICommand>> m_redoStack;
    size_t m_maxHistorySize;

    void trimHistory(); // Remove oldest commands if over limit
};

/**
 * @class MacroCommand
 * @brief Composite command for grouping multiple commands
 *
 * Executes multiple commands as a single atomic operation.
 * Useful for complex operations that should be undone together.
 *
 * @par Example:
 * @code
 * auto macro = std::make_unique<MacroCommand>("Apply Filters");
 * macro->addCommand(std::make_unique<ClearEventsCommand>(...));
 * macro->addCommand(std::make_unique<AddFilterCommand>(...));
 * macro->addCommand(std::make_unique<ApplyFiltersCommand>(...));
 * commandManager.execute(std::move(macro));
 * @endcode
 */
class MacroCommand : public ICommand {
public:
    explicit MacroCommand(std::string description);

    void execute() override;
    void undo() override;
    std::string getDescription() const override { return m_description; }

    /**
     * @brief Add command to macro
     * @param command Command to add (takes ownership)
     *
     * Commands are executed in order added.
     * Undo executes in reverse order.
     */
    void addCommand(std::unique_ptr<ICommand> command);

    /**
     * @brief Get number of sub-commands
     */
    size_t getCommandCount() const { return m_commands.size(); }

private:
    std::string m_description;
    std::vector<std::unique_ptr<ICommand>> m_commands;
    bool m_executed = false;
};

} // namespace command
