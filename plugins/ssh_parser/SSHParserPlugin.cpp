/**
 * @file SSHParserPlugin.cpp
 * @brief SSH Parser Plugin implementation
 * @author LogViewer Development Team
 * @date 2025
 */

#include "SSHParserPlugin.hpp"
#include "SSHConnectionWidget.hpp"
#include "SSHTerminalWidget.hpp"
#include "Logger.hpp"
#include <QWidget>
#include <algorithm>

namespace ssh
{

SSHParserPlugin::SSHParserPlugin()
    : m_status(plugin::PluginStatus::Unloaded)
    , m_isLicensed(false)
{
}

SSHParserPlugin::~SSHParserPlugin()
{
    Shutdown();
}

bool SSHParserPlugin::Initialize()
{
    util::Logger::Info("SSHParserPlugin: Initializing...");

    try
    {
        // Initialize libssh if needed
        // ssh_init() is called automatically in libssh 0.8+
        
        // Create parser instance
        m_parser = std::make_unique<SSHTextParser>();
        
        m_status = plugin::PluginStatus::Initialized;
        util::Logger::Info("SSHParserPlugin: Initialization successful");
        return true;
    }
    catch (const std::exception& e)
    {
        m_lastError = std::string("Initialization failed: ") + e.what();
        m_status = plugin::PluginStatus::Error;
        util::Logger::Error("SSHParserPlugin: {}", m_lastError);
        return false;
    }
}

void SSHParserPlugin::Shutdown()
{
    if (m_status == plugin::PluginStatus::Unloaded)
        return;

    util::Logger::Info("SSHParserPlugin: Shutting down...");

    try
    {
        // Cleanup is handled by individual SSH objects
        // ssh_finalize() is called automatically in libssh 0.8+
        
        m_status = plugin::PluginStatus::Unloaded;
        util::Logger::Info("SSHParserPlugin: Shutdown complete");
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("SSHParserPlugin: Shutdown error: {}", e.what());
    }
}

plugin::PluginMetadata SSHParserPlugin::GetMetadata() const
{
    plugin::PluginMetadata metadata;
    
    metadata.id = "ssh_parser";
    metadata.name = "SSH Parser Plugin";
    metadata.version = "1.0.0";
    metadata.author = "LogViewer Development Team";
    metadata.description = "Provides SSH connectivity and remote log parsing capabilities. "
                          "Allows connecting to remote systems via SSH and parsing log output "
                          "in real-time from command output, file tailing, or interactive shells.";
    metadata.website = "https://github.com/bula173/LogViewer";
    metadata.type = plugin::PluginType::Connector;
    metadata.requiresLicense = true;
    metadata.dependencies = {}; // No dependencies on other plugins
    
    return metadata;
}

plugin::PluginStatus SSHParserPlugin::GetStatus() const
{
    return m_status;
}

std::string SSHParserPlugin::GetLastError() const
{
    return m_lastError;
}

bool SSHParserPlugin::IsLicensed() const
{
    return m_isLicensed;
}

bool SSHParserPlugin::SetLicense(const std::string& licenseKey)
{
    // TODO: Implement license validation
    // For now, accept any non-empty license key
    if (licenseKey.empty())
    {
        m_lastError = "License key cannot be empty";
        return false;
    }

    m_licenseKey = licenseKey;
    m_isLicensed = true;
    
    util::Logger::Info("SSHParserPlugin: License key set");
    return true;
}

bool SSHParserPlugin::ValidateConfiguration() const
{
    // SSH parser has minimal configuration requirements
    // It's valid if initialized
    return m_status == plugin::PluginStatus::Initialized || 
           m_status == plugin::PluginStatus::Active;
}

parser::IDataParser* SSHParserPlugin::GetParserInterface()
{
    return m_parser.get();
}

bool SSHParserPlugin::SupportsExtension(const std::string& extension) const
{
    // SSH parser supports text-based log files
    static const std::vector<std::string> supportedExtensions = {
        ".log", ".txt", ".ssh", ".auth"
    };
    
    return std::find(supportedExtensions.begin(), 
                    supportedExtensions.end(), 
                    extension) != supportedExtensions.end();
}

std::vector<std::string> SSHParserPlugin::GetSupportedExtensions() const
{
    return {".log", ".txt", ".ssh", ".auth"};
}

QWidget* SSHParserPlugin::CreateTab(QWidget* parent)
{
    util::Logger::Info("SSHParserPlugin: Creating SSH terminal tab");
    // Return terminal widget for the main panel
    auto* terminalWidget = new ssh::SSHTerminalWidget(parent);
    
    // TODO: Wire up signals from config widget to terminal widget
    // This would typically be done by the MainWindow or a manager class
    
    return terminalWidget;
}

QWidget* SSHParserPlugin::GetConfigurationUI()
{
    util::Logger::Info("SSHParserPlugin: Creating SSH configuration UI");
    // Return connection configuration widget for the left panel
    auto* configWidget = new ssh::SSHConnectionWidget();
    
    // TODO: Store reference to config widget to wire up signals later
    // or emit signals through plugin manager
    
    return configWidget;
}

std::unique_ptr<SSHConnection> SSHParserPlugin::CreateConnection(
    const SSHConnectionConfig& config)
{
    if (!m_isLicensed)
    {
        util::Logger::Warn("SSHParserPlugin: Attempting to create connection without license");
        m_lastError = "Plugin not licensed";
        return nullptr;
    }

    if (m_status != plugin::PluginStatus::Initialized && 
        m_status != plugin::PluginStatus::Active)
    {
        util::Logger::Warn("SSHParserPlugin: Plugin not initialized");
        m_lastError = "Plugin not initialized";
        return nullptr;
    }

    try
    {
        auto connection = std::make_unique<SSHConnection>(config);
        m_status = plugin::PluginStatus::Active;
        return connection;
    }
    catch (const std::exception& e)
    {
        m_lastError = std::string("Failed to create connection: ") + e.what();
        util::Logger::Error("SSHParserPlugin: {}", m_lastError);
        return nullptr;
    }
}

std::unique_ptr<SSHTextParser> SSHParserPlugin::CreateTextParser()
{
    if (!m_isLicensed)
    {
        util::Logger::Warn("SSHParserPlugin: Attempting to create parser without license");
        m_lastError = "Plugin not licensed";
        return nullptr;
    }

    if (m_status != plugin::PluginStatus::Initialized && 
        m_status != plugin::PluginStatus::Active)
    {
        util::Logger::Warn("SSHParserPlugin: Plugin not initialized");
        m_lastError = "Plugin not initialized";
        return nullptr;
    }

    try
    {
        auto parser = std::make_unique<SSHTextParser>();
        m_status = plugin::PluginStatus::Active;
        return parser;
    }
    catch (const std::exception& e)
    {
        m_lastError = std::string("Failed to create parser: ") + e.what();
        util::Logger::Error("SSHParserPlugin: {}", m_lastError);
        return nullptr;
    }
}

std::unique_ptr<SSHLogSource> SSHParserPlugin::CreateLogSource(
    const SSHLogSourceConfig& config)
{
    if (!m_isLicensed)
    {
        util::Logger::Warn("SSHParserPlugin: Attempting to create log source without license");
        m_lastError = "Plugin not licensed";
        return nullptr;
    }

    if (m_status != plugin::PluginStatus::Initialized && 
        m_status != plugin::PluginStatus::Active)
    {
        util::Logger::Warn("SSHParserPlugin: Plugin not initialized");
        m_lastError = "Plugin not initialized";
        return nullptr;
    }

    try
    {
        auto logSource = std::make_unique<SSHLogSource>(config);
        m_status = plugin::PluginStatus::Active;
        return logSource;
    }
    catch (const std::exception& e)
    {
        m_lastError = std::string("Failed to create log source: ") + e.what();
        util::Logger::Error("SSHParserPlugin: {}", m_lastError);
        return nullptr;
    }
}

} // namespace ssh

// Plugin factory function implementation
extern "C" {
    EXPORT_PLUGIN(ssh::SSHParserPlugin)
}
