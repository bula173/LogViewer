/**
 * @file SSHLogSource.hpp
 * @brief Integrated SSH log source combining connection and parsing
 * @author LogViewer Development Team
 * @date 2025
 */

#ifndef SSH_LOG_SOURCE_HPP
#define SSH_LOG_SOURCE_HPP

#include "SSHConnection.hpp"
#include "SSHTextParser.hpp"
#include "application/parser/IDataParser.hpp"
#include <atomic>
#include <memory>
#include <thread>

namespace ssh
{

/**
 * @enum SSHLogSourceMode
 * @brief Mode of operation for SSH log source
 */
enum class SSHLogSourceMode
{
    CommandOutput,    ///< Execute command and parse output
    TailFile,         ///< Tail a remote log file (tail -f)
    InteractiveShell  ///< Parse interactive shell output
};

/**
 * @struct SSHLogSourceConfig
 * @brief Configuration for SSH log source
 */
struct SSHLogSourceConfig
{
    SSHConnectionConfig connectionConfig;  ///< SSH connection parameters
    SSHLogSourceMode mode = SSHLogSourceMode::TailFile;
    std::string command;                   ///< Command to execute (for CommandOutput mode)
    std::string logFilePath;               ///< Remote log file path (for TailFile mode)
    int pollIntervalMs = 100;              ///< Polling interval in milliseconds
    bool autoReconnect = true;             ///< Auto-reconnect on connection loss
};

/**
 * @class SSHLogSource
 * @brief Manages SSH connection and real-time log parsing
 * 
 * This class integrates:
 * - SSH connection management
 * - Real-time log retrieval from remote host
 * - Text parsing of SSH output
 * - Event notification to observers
 */
class SSHLogSource
{
public:
    /**
     * @brief Constructs SSH log source with configuration
     * @param config Log source configuration
     */
    explicit SSHLogSource(const SSHLogSourceConfig& config);

    /**
     * @brief Destructor - stops monitoring and disconnects
     */
    ~SSHLogSource();

    // Disable copy, allow move
    SSHLogSource(const SSHLogSource&) = delete;
    SSHLogSource& operator=(const SSHLogSource&) = delete;
    SSHLogSource(SSHLogSource&&) noexcept;
    SSHLogSource& operator=(SSHLogSource&&) noexcept;

    /**
     * @brief Starts log monitoring
     * @return True if started successfully
     */
    bool Start();

    /**
     * @brief Stops log monitoring
     */
    void Stop();

    /**
     * @brief Checks if currently monitoring
     * @return True if monitoring active
     */
    bool IsRunning() const;

    /**
     * @brief Gets the text parser for configuration
     * @return Reference to parser
     */
    SSHTextParser& GetParser();

    /**
     * @brief Gets the SSH connection
     * @return Reference to connection
     */
    SSHConnection& GetConnection();

    /**
     * @brief Registers an observer for parsed events
     * @param observer Observer to notify
     */
    void RegisterObserver(parser::IDataParserObserver* observer);

    /**
     * @brief Unregisters an observer
     * @param observer Observer to remove
     */
    void UnregisterObserver(parser::IDataParserObserver* observer);

    /**
     * @brief Gets the last error message
     * @return Error description
     */
    std::string GetLastError() const;

private:
    /**
     * @brief Main monitoring thread function
     */
    void MonitoringThread();

    /**
     * @brief Monitors command output mode
     */
    void MonitorCommandOutput();

    /**
     * @brief Monitors tail file mode
     */
    void MonitorTailFile();

    /**
     * @brief Monitors interactive shell mode
     */
    void MonitorInteractiveShell();

    /**
     * @brief Processes received data buffer
     * @param buffer Data buffer
     * @param size Size of data
     */
    void ProcessData(const char* buffer, size_t size);

    SSHLogSourceConfig m_config;              ///< Configuration
    std::unique_ptr<SSHConnection> m_connection; ///< SSH connection
    std::unique_ptr<SSHTextParser> m_parser;  ///< Text parser
    std::unique_ptr<std::thread> m_monitorThread; ///< Monitoring thread
    std::atomic<bool> m_running{false};       ///< Running state
    std::string m_lastError;                  ///< Last error message
    std::string m_lineBuffer;                 ///< Line accumulation buffer
};

} // namespace ssh

#endif // SSH_LOG_SOURCE_HPP
