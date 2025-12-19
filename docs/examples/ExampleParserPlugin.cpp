/**
 * @file ExampleParserPlugin.cpp
 * @brief Example parser plugin implementation
 * @author LogViewer Development Team
 * @date 2025
 */

#include "ExampleParserPlugin.hpp"
#include "Logger.hpp"
#include <fstream>

namespace example
{

ExampleParserPlugin::ExampleParserPlugin()
    : m_status(plugin::PluginStatus::Unloaded)
    , m_isLicensed(false)
    , m_parsingInProgress(false)
    , m_totalEvents(0)
    , m_parsedEvents(0)
{
}

ExampleParserPlugin::~ExampleParserPlugin()
{
    Shutdown();
}

// =============================================================================
// IPlugin interface implementation
// =============================================================================

bool ExampleParserPlugin::Initialize()
{
    util::Logger::Info("ExampleParserPlugin: Initializing...");

    try
    {
        // Initialize plugin resources
        // For example: load configuration, allocate buffers, etc.
        
        m_status = plugin::PluginStatus::Initialized;
        util::Logger::Info("ExampleParserPlugin: Initialization successful");
        return true;
    }
    catch (const std::exception& e)
    {
        m_lastError = std::string("Initialization failed: ") + e.what();
        m_status = plugin::PluginStatus::Error;
        util::Logger::Error("ExampleParserPlugin: {}", m_lastError);
        return false;
    }
}

void ExampleParserPlugin::Shutdown()
{
    if (m_status == plugin::PluginStatus::Unloaded)
        return;

    util::Logger::Info("ExampleParserPlugin: Shutting down...");

    try
    {
        // Cancel any ongoing parsing
        if (m_parsingInProgress)
        {
            CancelParsing();
        }
        
        // Clear observers
        m_observers.clear();
        
        m_status = plugin::PluginStatus::Unloaded;
        util::Logger::Info("ExampleParserPlugin: Shutdown complete");
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("ExampleParserPlugin: Shutdown error: {}", e.what());
    }
}

plugin::PluginMetadata ExampleParserPlugin::GetMetadata() const
{
    plugin::PluginMetadata metadata;
    
    metadata.id = "example_parser";
    metadata.name = "Example Parser Plugin";
    metadata.version = "1.0.0";
    metadata.author = "LogViewer Development Team";
    metadata.description = "Example parser plugin demonstrating IPlugin + IDataParser implementation. "
                          "Parses custom log format with .example extension.";
    metadata.website = "https://github.com/bula173/LogViewer";
    metadata.type = plugin::PluginType::Parser;  // MUST be Parser type
    metadata.requiresLicense = false;  // Set to true for commercial plugins
    metadata.dependencies = {};
    
    return metadata;
}

plugin::PluginStatus ExampleParserPlugin::GetStatus() const
{
    return m_status;
}

std::string ExampleParserPlugin::GetLastError() const
{
    return m_lastError;
}

bool ExampleParserPlugin::IsLicensed() const
{
    // If plugin doesn't require license, always return true
    auto metadata = GetMetadata();
    if (!metadata.requiresLicense)
        return true;
    
    return m_isLicensed;
}

bool ExampleParserPlugin::SetLicense(const std::string& licenseKey)
{
    // Example license validation
    // In production, implement proper license verification
    if (licenseKey.empty())
    {
        m_lastError = "License key cannot be empty";
        return false;
    }

    // TODO: Implement actual license validation
    // For example: check signature, expiry date, allowed features, etc.
    
    m_licenseKey = licenseKey;
    m_isLicensed = true;
    
    util::Logger::Info("ExampleParserPlugin: License key set");
    return true;
}

bool ExampleParserPlugin::SupportsExtension(const std::string& extension) const
{
    // Check if this plugin can handle the given file extension
    return extension == ".example" || extension == ".exmpl";
}

std::vector<std::string> ExampleParserPlugin::GetSupportedExtensions() const
{
    // Return list of file extensions this parser supports
    return {".example", ".exmpl"};
}

// =============================================================================
// IDataParser interface implementation
// =============================================================================

void ExampleParserPlugin::Parse(const std::filesystem::path& path)
{
    util::Logger::Info("ExampleParserPlugin: Parsing file: {}", path.string());

    if (!IsLicensed())
    {
        m_lastError = "Plugin not licensed";
        util::Logger::Error("ExampleParserPlugin: {}", m_lastError);
        return;
    }

    if (m_status != plugin::PluginStatus::Initialized && 
        m_status != plugin::PluginStatus::Active)
    {
        m_lastError = "Plugin not initialized";
        util::Logger::Error("ExampleParserPlugin: {}", m_lastError);
        return;
    }

    std::ifstream file(path);
    if (!file.is_open())
    {
        m_lastError = "Failed to open file: " + path.string();
        util::Logger::Error("ExampleParserPlugin: {}", m_lastError);
        return;
    }

    ParseStream(file);
}

void ExampleParserPlugin::ParseStream(std::istream& stream)
{
    util::Logger::Info("ExampleParserPlugin: Parsing stream...");

    m_parsingInProgress = true;
    m_parsedEvents = 0;
    m_status = plugin::PluginStatus::Active;

    try
    {
        std::string line;
        int eventId = 1;
        
        while (std::getline(stream, line) && m_parsingInProgress)
        {
            // Example parsing logic - adapt to your format
            // Format: TIMESTAMP|LEVEL|MESSAGE
            
            if (line.empty() || line[0] == '#')
                continue;  // Skip empty lines and comments
            
            db::LogEvent::EventItems items;
            
            // Simple parsing - split by '|'
            size_t pos1 = line.find('|');
            size_t pos2 = line.find('|', pos1 + 1);
            
            if (pos1 != std::string::npos && pos2 != std::string::npos)
            {
                std::string timestamp = line.substr(0, pos1);
                std::string level = line.substr(pos1 + 1, pos2 - pos1 - 1);
                std::string message = line.substr(pos2 + 1);
                
                items["timestamp"] = timestamp;
                items["level"] = level;
                items["message"] = message;
                items["id"] = std::to_string(eventId);
                
                // Create event and notify observers
                db::LogEvent event(eventId, items);
                NotifyNewEvent(std::move(event));
                
                m_parsedEvents++;
                eventId++;
                
                // Update progress periodically
                if (m_parsedEvents % 100 == 0)
                {
                    NotifyProgress();
                }
            }
        }
        
        m_totalEvents = m_parsedEvents;
        m_parsingInProgress = false;
        
        util::Logger::Info("ExampleParserPlugin: Parsing complete. Events: {}", 
            m_parsedEvents);
    }
    catch (const std::exception& e)
    {
        m_lastError = std::string("Parsing error: ") + e.what();
        m_status = plugin::PluginStatus::Error;
        m_parsingInProgress = false;
        util::Logger::Error("ExampleParserPlugin: {}", m_lastError);
    }
}

void ExampleParserPlugin::RegisterObserver(parser::IDataParserObserver* observer)
{
    if (observer && std::find(m_observers.begin(), m_observers.end(), observer) == m_observers.end())
    {
        m_observers.push_back(observer);
    }
}

void ExampleParserPlugin::UnregisterObserver(parser::IDataParserObserver* observer)
{
    auto it = std::find(m_observers.begin(), m_observers.end(), observer);
    if (it != m_observers.end())
    {
        m_observers.erase(it);
    }
}

void ExampleParserPlugin::ClearData()
{
    m_parsedEvents = 0;
    m_totalEvents = 0;
}

bool ExampleParserPlugin::IsParsingInProgress() const
{
    return m_parsingInProgress;
}

void ExampleParserPlugin::CancelParsing()
{
    m_parsingInProgress = false;
    util::Logger::Info("ExampleParserPlugin: Parsing cancelled");
}

std::uint64_t ExampleParserPlugin::GetTotalEvents() const
{
    return m_totalEvents;
}

std::uint64_t ExampleParserPlugin::GetParsedEvents() const
{
    return m_parsedEvents;
}

// =============================================================================
// Helper methods
// =============================================================================

void ExampleParserPlugin::NotifyProgress()
{
    for (auto* observer : m_observers)
    {
        observer->ProgressUpdated();
    }
}

void ExampleParserPlugin::NotifyNewEvent(db::LogEvent&& event)
{
    for (auto* observer : m_observers)
    {
        observer->NewEventFound(std::move(event));
    }
}

} // namespace example

// =============================================================================
// Plugin factory function
// =============================================================================

extern "C" EXPORT_PLUGIN std::unique_ptr<plugin::IPlugin> CreatePlugin()
{
    return std::make_unique<example::ExampleParserPlugin>();
}
