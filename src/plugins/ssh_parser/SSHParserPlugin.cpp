/**
 * @file SSHParserPlugin.cpp
 * @brief SSH Parser Plugin implementation
 * @author LogViewer Development Team
 * @date 2025
 */

#include "SSHParserPlugin.hpp"
#include "util/Logger.hpp"

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

std::unique_ptr<SSHConnection> SSHParserPlugin::CreateConnection(
    const std::string& hostname,
    int port,
    const std::string& username)
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
        auto connection = std::make_unique<SSHConnection>(hostname, port, username);
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
    std::unique_ptr<SSHConnection> connection,
    std::unique_ptr<SSHTextParser> parser,
    SSHLogSource::MonitorMode mode)
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

    if (!connection || !parser)
    {
        m_lastError = "Connection and parser cannot be null";
        return nullptr;
    }

    try
    {
        auto logSource = std::make_unique<SSHLogSource>(
            std::move(connection), 
            std::move(parser), 
            mode);
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
extern "C" EXPORT_PLUGIN std::unique_ptr<plugin::IPlugin> CreatePlugin()
{
    return std::make_unique<ssh::SSHParserPlugin>();
}
