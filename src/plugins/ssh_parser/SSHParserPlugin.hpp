/**
 * @file SSHParserPlugin.hpp
 * @brief SSH Parser Plugin implementation
 * @author LogViewer Development Team
 * @date 2025
 */

#pragma once

#include "application/plugins/IPlugin.hpp"
#include "SSHConnection.hpp"
#include "SSHTextParser.hpp"
#include "SSHLogSource.hpp"
#include <memory>
#include <string>

namespace ssh
{

/**
 * @brief SSH Parser Plugin
 * 
 * Plugin that provides SSH connectivity and remote log parsing capabilities.
 * This plugin allows connecting to remote systems via SSH and parsing log
 * output in real-time.
 */
class SSHParserPlugin : public plugin::IPlugin
{
public:
    SSHParserPlugin();
    ~SSHParserPlugin() override;

    // IPlugin interface implementation
    bool Initialize() override;
    void Shutdown() override;
    plugin::PluginMetadata GetMetadata() const override;
    plugin::PluginStatus GetStatus() const override;
    std::string GetLastError() const override;
    bool IsLicensed() const override;
    bool SetLicense(const std::string& licenseKey) override;

    /**
     * @brief Create SSH connection
     * @param hostname Remote host address
     * @param port SSH port (default: 22)
     * @param username Username for authentication
     * @return Unique pointer to SSH connection
     */
    std::unique_ptr<SSHConnection> CreateConnection(
        const std::string& hostname,
        int port,
        const std::string& username);

    /**
     * @brief Create SSH text parser
     * @return Unique pointer to SSH text parser
     */
    std::unique_ptr<SSHTextParser> CreateTextParser();

    /**
     * @brief Create SSH log source
     * @param connection SSH connection
     * @param parser Text parser
     * @param mode Monitoring mode
     * @return Unique pointer to SSH log source
     */
    std::unique_ptr<SSHLogSource> CreateLogSource(
        std::unique_ptr<SSHConnection> connection,
        std::unique_ptr<SSHTextParser> parser,
        SSHLogSource::MonitorMode mode);

private:
    plugin::PluginStatus m_status;
    std::string m_lastError;
    std::string m_licenseKey;
    bool m_isLicensed;
};

} // namespace ssh

// Plugin factory function
extern "C" EXPORT_PLUGIN std::unique_ptr<plugin::IPlugin> CreatePlugin();
