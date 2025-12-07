/**
 * @file SSHConnection.hpp
 * @brief SSH connection management for remote log access
 * @author LogViewer Development Team
 * @date 2025
 * 
 * This module provides SSH connectivity to remote test computers,
 * enabling real-time log parsing from SSH terminal output.
 * 
 * @note This is an optional feature available only for specific customers
 */

#ifndef SSH_CONNECTION_HPP
#define SSH_CONNECTION_HPP

#include <libssh/libssh.h>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ssh
{

/**
 * @enum SSHAuthMethod
 * @brief Supported authentication methods for SSH connections
 */
enum class SSHAuthMethod
{
    Password,      ///< Username and password authentication
    PublicKey,     ///< SSH key-based authentication
    Agent,         ///< SSH agent authentication
    Interactive    ///< Interactive keyboard authentication
};

/**
 * @struct SSHConnectionConfig
 * @brief Configuration parameters for SSH connection
 */
struct SSHConnectionConfig
{
    std::string hostname;           ///< Remote host address
    int port = 22;                  ///< SSH port (default: 22)
    std::string username;           ///< Username for authentication
    std::string password;           ///< Password (for password auth)
    std::string privateKeyPath;     ///< Path to private key file
    std::string publicKeyPath;      ///< Path to public key file
    std::string passphrase;         ///< Passphrase for encrypted key
    SSHAuthMethod authMethod = SSHAuthMethod::Password;
    int timeoutSeconds = 30;        ///< Connection timeout
    bool strictHostKeyChecking = true; ///< Verify host key
};

/**
 * @class SSHConnection
 * @brief Manages SSH connection to remote test computer
 * 
 * This class handles:
 * - Establishing SSH connection
 * - Authentication (password, key, agent)
 * - Executing remote commands
 * - Reading terminal output
 * - Session management
 */
class SSHConnection
{
public:
    /**
     * @brief Constructs SSH connection with configuration
     * @param config Connection configuration parameters
     */
    explicit SSHConnection(const SSHConnectionConfig& config);
    
    /**
     * @brief Destructor - ensures clean disconnection
     */
    ~SSHConnection();

    // Disable copy, allow move
    SSHConnection(const SSHConnection&) = delete;
    SSHConnection& operator=(const SSHConnection&) = delete;
    SSHConnection(SSHConnection&&) noexcept;
    SSHConnection& operator=(SSHConnection&&) noexcept;

    /**
     * @brief Establishes connection to remote host
     * @return True if connection successful, false otherwise
     */
    bool Connect();

    /**
     * @brief Closes the SSH connection
     */
    void Disconnect();

    /**
     * @brief Checks if currently connected
     * @return True if connected, false otherwise
     */
    bool IsConnected() const;

    /**
     * @brief Executes a command on remote host
     * @param command Command to execute
     * @param output Output from command execution
     * @return Exit code of the command
     */
    int ExecuteCommand(const std::string& command, std::string& output);

    /**
     * @brief Opens an interactive shell channel
     * @return True if shell opened successfully
     */
    bool OpenShell();

    /**
     * @brief Reads data from the shell channel
     * @param buffer Buffer to store read data
     * @param maxSize Maximum bytes to read
     * @return Number of bytes read, -1 on error
     */
    int ReadShellOutput(char* buffer, size_t maxSize);

    /**
     * @brief Writes data to the shell channel
     * @param data Data to write
     * @return Number of bytes written, -1 on error
     */
    int WriteShellInput(const std::string& data);

    /**
     * @brief Closes the shell channel
     */
    void CloseShell();

    /**
     * @brief Gets the last error message
     * @return Error description
     */
    std::string GetLastError() const;

    /**
     * @brief Gets connection configuration
     * @return Current configuration
     */
    const SSHConnectionConfig& GetConfig() const { return m_config; }

private:
    /**
     * @brief Performs authentication based on configured method
     * @return True if authentication successful
     */
    bool Authenticate();

    /**
     * @brief Authenticates using password
     * @return True if successful
     */
    bool AuthenticateWithPassword();

    /**
     * @brief Authenticates using public key
     * @return True if successful
     */
    bool AuthenticateWithPublicKey();

    /**
     * @brief Authenticates using SSH agent
     * @return True if successful
     */
    bool AuthenticateWithAgent();

    SSHConnectionConfig m_config;     ///< Connection configuration
    ssh_session m_session = nullptr;  ///< libssh session handle
    ssh_channel m_channel = nullptr;  ///< libssh channel handle
    std::string m_lastError;          ///< Last error message
    bool m_connected = false;         ///< Connection state
};

} // namespace ssh

#endif // SSH_CONNECTION_HPP
