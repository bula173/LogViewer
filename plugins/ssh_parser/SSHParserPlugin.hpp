/**
 * @file SSHParserPlugin.hpp
 * @brief SSH Parser Plugin implementation
 * @author LogViewer Development Team
 * @date 2025
 */

#pragma once

#include "IPlugin.hpp"
#include "SSHConnection.hpp"
#include "SSHTextParser.hpp"
#include "SSHLogSource.hpp"
#include "SSHConnectionWidget.hpp"
#include <memory>
#include <string>

class QWidget;

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
    bool ValidateConfiguration() const override;
    
    // Parser plugin interface
    parser::IDataParser* GetParserInterface() override;
    bool SupportsExtension(const std::string& extension) const override;
    std::vector<std::string> GetSupportedExtensions() const override;
    QWidget* CreateTab(QWidget* parent) override;
    QWidget* GetConfigurationUI() override;

    /**
     * @brief Create SSH connection
     * @param config SSH connection configuration
     * @return Unique pointer to SSH connection
     */
    std::unique_ptr<SSHConnection> CreateConnection(
        const SSHConnectionConfig& config);

    /**
     * @brief Create SSH text parser
     * @return Unique pointer to SSH text parser
     */
    std::unique_ptr<SSHTextParser> CreateTextParser();

    /**
     * @brief Create SSH log source
     * @param config SSH log source configuration
     * @return Unique pointer to SSH log source
     */
    std::unique_ptr<SSHLogSource> CreateLogSource(
        const SSHLogSourceConfig& config);

private:
    plugin::PluginStatus m_status;
    std::string m_lastError;
    std::string m_licenseKey;
    bool m_isLicensed;
    std::unique_ptr<SSHTextParser> m_parser;
};

} // namespace ssh
