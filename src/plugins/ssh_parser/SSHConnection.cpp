/**
 * @file SSHConnection.cpp
 * @brief Implementation of SSH connection management
 * @author LogViewer Development Team
 * @date 2025
 */

#include "SSHConnection.hpp"
#include "util/Logger.hpp"
#include <cstring>

namespace ssh
{

SSHConnection::SSHConnection(const SSHConnectionConfig& config)
    : m_config(config)
{
    util::Logger::Debug("SSHConnection constructor called for host: {}", config.hostname);
}

SSHConnection::~SSHConnection()
{
    Disconnect();
}

SSHConnection::SSHConnection(SSHConnection&& other) noexcept
    : m_config(std::move(other.m_config))
    , m_session(other.m_session)
    , m_channel(other.m_channel)
    , m_lastError(std::move(other.m_lastError))
    , m_connected(other.m_connected)
{
    other.m_session = nullptr;
    other.m_channel = nullptr;
    other.m_connected = false;
}

SSHConnection& SSHConnection::operator=(SSHConnection&& other) noexcept
{
    if (this != &other)
    {
        Disconnect();
        
        m_config = std::move(other.m_config);
        m_session = other.m_session;
        m_channel = other.m_channel;
        m_lastError = std::move(other.m_lastError);
        m_connected = other.m_connected;
        
        other.m_session = nullptr;
        other.m_channel = nullptr;
        other.m_connected = false;
    }
    return *this;
}

bool SSHConnection::Connect()
{
    util::Logger::Info("Connecting to SSH host: {}:{}", m_config.hostname, m_config.port);

    // Create SSH session
    m_session = ssh_new();
    if (!m_session)
    {
        m_lastError = "Failed to create SSH session";
        util::Logger::Error("{}", m_lastError);
        return false;
    }

    // Set connection options
    ssh_options_set(m_session, SSH_OPTIONS_HOST, m_config.hostname.c_str());
    ssh_options_set(m_session, SSH_OPTIONS_PORT, &m_config.port);
    ssh_options_set(m_session, SSH_OPTIONS_USER, m_config.username.c_str());
    
    long timeout = m_config.timeoutSeconds;
    ssh_options_set(m_session, SSH_OPTIONS_TIMEOUT, &timeout);

    // Connect to server
    int rc = ssh_connect(m_session);
    if (rc != SSH_OK)
    {
        m_lastError = std::string("SSH connection failed: ") + ssh_get_error(m_session);
        util::Logger::Error("{}", m_lastError);
        ssh_free(m_session);
        m_session = nullptr;
        return false;
    }

    // Verify host key if strict checking enabled
    if (m_config.strictHostKeyChecking)
    {
        ssh_key srv_pubkey;
        rc = ssh_get_server_publickey(m_session, &srv_pubkey);
        if (rc != SSH_OK)
        {
            m_lastError = "Failed to get server public key";
            util::Logger::Error("{}", m_lastError);
            Disconnect();
            return false;
        }

        enum ssh_known_hosts_e state = ssh_session_is_known_server(m_session);
        ssh_key_free(srv_pubkey);

        if (state != SSH_KNOWN_HOSTS_OK && state != SSH_KNOWN_HOSTS_CHANGED)
        {
            util::Logger::Warn("SSH host key verification state: {}", static_cast<int>(state));
        }
    }

    // Authenticate
    if (!Authenticate())
    {
        util::Logger::Error("SSH authentication failed");
        Disconnect();
        return false;
    }

    m_connected = true;
    util::Logger::Info("SSH connection established successfully");
    return true;
}

void SSHConnection::Disconnect()
{
    if (m_channel)
    {
        CloseShell();
    }

    if (m_session)
    {
        ssh_disconnect(m_session);
        ssh_free(m_session);
        m_session = nullptr;
    }

    m_connected = false;
    util::Logger::Debug("SSH connection closed");
}

bool SSHConnection::IsConnected() const
{
    return m_connected && m_session != nullptr;
}

bool SSHConnection::Authenticate()
{
    switch (m_config.authMethod)
    {
        case SSHAuthMethod::Password:
            return AuthenticateWithPassword();
        
        case SSHAuthMethod::PublicKey:
            return AuthenticateWithPublicKey();
        
        case SSHAuthMethod::Agent:
            return AuthenticateWithAgent();
        
        case SSHAuthMethod::Interactive:
            // Interactive authentication not yet implemented
            m_lastError = "Interactive authentication not supported";
            return false;
        
        default:
            m_lastError = "Unknown authentication method";
            return false;
    }
}

bool SSHConnection::AuthenticateWithPassword()
{
    util::Logger::Debug("Authenticating with password");
    
    int rc = ssh_userauth_password(m_session, nullptr, m_config.password.c_str());
    if (rc != SSH_AUTH_SUCCESS)
    {
        m_lastError = std::string("Password authentication failed: ") + ssh_get_error(m_session);
        util::Logger::Error("{}", m_lastError);
        return false;
    }

    util::Logger::Info("Password authentication successful");
    return true;
}

bool SSHConnection::AuthenticateWithPublicKey()
{
    util::Logger::Debug("Authenticating with public key");

    ssh_key privkey;
    int rc = ssh_pki_import_privkey_file(
        m_config.privateKeyPath.c_str(),
        m_config.passphrase.empty() ? nullptr : m_config.passphrase.c_str(),
        nullptr, nullptr, &privkey);

    if (rc != SSH_OK)
    {
        m_lastError = "Failed to load private key";
        util::Logger::Error("{}", m_lastError);
        return false;
    }

    rc = ssh_userauth_publickey(m_session, nullptr, privkey);
    ssh_key_free(privkey);

    if (rc != SSH_AUTH_SUCCESS)
    {
        m_lastError = std::string("Public key authentication failed: ") + ssh_get_error(m_session);
        util::Logger::Error("{}", m_lastError);
        return false;
    }

    util::Logger::Info("Public key authentication successful");
    return true;
}

bool SSHConnection::AuthenticateWithAgent()
{
    util::Logger::Debug("Authenticating with SSH agent");

    int rc = ssh_userauth_agent(m_session, nullptr);
    if (rc != SSH_AUTH_SUCCESS)
    {
        m_lastError = std::string("Agent authentication failed: ") + ssh_get_error(m_session);
        util::Logger::Error("{}", m_lastError);
        return false;
    }

    util::Logger::Info("Agent authentication successful");
    return true;
}

int SSHConnection::ExecuteCommand(const std::string& command, std::string& output)
{
    if (!IsConnected())
    {
        m_lastError = "Not connected";
        return -1;
    }

    util::Logger::Debug("Executing SSH command: {}", command);

    ssh_channel channel = ssh_channel_new(m_session);
    if (!channel)
    {
        m_lastError = "Failed to create channel";
        return -1;
    }

    int rc = ssh_channel_open_session(channel);
    if (rc != SSH_OK)
    {
        m_lastError = "Failed to open channel session";
        ssh_channel_free(channel);
        return -1;
    }

    rc = ssh_channel_request_exec(channel, command.c_str());
    if (rc != SSH_OK)
    {
        m_lastError = "Failed to execute command";
        ssh_channel_close(channel);
        ssh_channel_free(channel);
        return -1;
    }

    // Read output
    char buffer[4096];
    int nbytes;
    output.clear();

    while ((nbytes = ssh_channel_read(channel, buffer, sizeof(buffer), 0)) > 0)
    {
        output.append(buffer, nbytes);
    }

    // Get exit status
    int exitCode = ssh_channel_get_exit_status(channel);

    ssh_channel_send_eof(channel);
    ssh_channel_close(channel);
    ssh_channel_free(channel);

    util::Logger::Debug("Command executed with exit code: {}", exitCode);
    return exitCode;
}

bool SSHConnection::OpenShell()
{
    if (!IsConnected())
    {
        m_lastError = "Not connected";
        return false;
    }

    if (m_channel)
    {
        m_lastError = "Shell already open";
        return false;
    }

    m_channel = ssh_channel_new(m_session);
    if (!m_channel)
    {
        m_lastError = "Failed to create channel";
        return false;
    }

    int rc = ssh_channel_open_session(m_channel);
    if (rc != SSH_OK)
    {
        m_lastError = "Failed to open channel session";
        ssh_channel_free(m_channel);
        m_channel = nullptr;
        return false;
    }

    rc = ssh_channel_request_pty(m_channel);
    if (rc != SSH_OK)
    {
        m_lastError = "Failed to request PTY";
        CloseShell();
        return false;
    }

    rc = ssh_channel_request_shell(m_channel);
    if (rc != SSH_OK)
    {
        m_lastError = "Failed to request shell";
        CloseShell();
        return false;
    }

    util::Logger::Info("SSH shell opened successfully");
    return true;
}

int SSHConnection::ReadShellOutput(char* buffer, size_t maxSize)
{
    if (!m_channel)
    {
        m_lastError = "Shell not open";
        return -1;
    }

    int nbytes = ssh_channel_read(m_channel, buffer, maxSize, 0);
    if (nbytes < 0)
    {
        m_lastError = "Failed to read from channel";
    }

    return nbytes;
}

int SSHConnection::WriteShellInput(const std::string& data)
{
    if (!m_channel)
    {
        m_lastError = "Shell not open";
        return -1;
    }

    int nbytes = ssh_channel_write(m_channel, data.c_str(), data.size());
    if (nbytes < 0)
    {
        m_lastError = "Failed to write to channel";
    }

    return nbytes;
}

void SSHConnection::CloseShell()
{
    if (m_channel)
    {
        ssh_channel_send_eof(m_channel);
        ssh_channel_close(m_channel);
        ssh_channel_free(m_channel);
        m_channel = nullptr;
        util::Logger::Debug("SSH shell closed");
    }
}

std::string SSHConnection::GetLastError() const
{
    return m_lastError;
}

} // namespace ssh
