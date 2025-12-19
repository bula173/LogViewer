/**
 * @file ExampleParserPlugin.hpp
 * @brief Example parser plugin implementation
 * @author LogViewer Development Team
 * @date 2025
 * 
 * This example demonstrates how to create a parser plugin that implements
 * both the IPlugin interface and the IDataParser interface.
 */

#pragma once

#include "IPlugin.hpp"
#include "IDataParser.hpp"
#include <memory>
#include <string>
#include <vector>

namespace example
{

/**
 * @brief Example Parser Plugin
 * 
 * This plugin demonstrates how to implement both IPlugin and IDataParser.
 * Parser plugins MUST implement both interfaces:
 * - IPlugin: Base plugin interface for lifecycle management
 * - IDataParser: Type-specific interface for parsing functionality
 */
class ExampleParserPlugin : public plugin::IPlugin, public parser::IDataParser
{
public:
    ExampleParserPlugin();
    ~ExampleParserPlugin() override;

    // =========================================================================
    // IPlugin interface implementation
    // =========================================================================
    
    bool Initialize() override;
    void Shutdown() override;
    plugin::PluginMetadata GetMetadata() const override;
    plugin::PluginStatus GetStatus() const override;
    std::string GetLastError() const override;
    bool IsLicensed() const override;
    bool SetLicense(const std::string& licenseKey) override;
    
    // Type-specific interface access for Parser plugins
    parser::IDataParser* GetParserInterface() override { return this; }
    
    // File extension support (for Parser plugins)
    bool SupportsExtension(const std::string& extension) const override;
    std::vector<std::string> GetSupportedExtensions() const override;
    
    bool ValidateConfiguration() const override { return true; }

    // =========================================================================
    // IDataParser interface implementation
    // =========================================================================
    
    void Parse(const std::filesystem::path& path) override;
    void ParseStream(std::istream& stream) override;
    void RegisterObserver(parser::IDataParserObserver* observer) override;
    void UnregisterObserver(parser::IDataParserObserver* observer) override;
    void ClearData() override;
    bool IsParsingInProgress() const override;
    void CancelParsing() override;
    std::uint64_t GetTotalEvents() const override;
    std::uint64_t GetParsedEvents() const override;

private:
    // Plugin state
    plugin::PluginStatus m_status;
    std::string m_lastError;
    std::string m_licenseKey;
    bool m_isLicensed;
    
    // Parser state
    std::vector<parser::IDataParserObserver*> m_observers;
    bool m_parsingInProgress;
    std::uint64_t m_totalEvents;
    std::uint64_t m_parsedEvents;
    
    // Helper methods
    void NotifyProgress();
    void NotifyNewEvent(db::LogEvent&& event);
};

} // namespace example

// Plugin factory function
extern "C" EXPORT_PLUGIN std::unique_ptr<plugin::IPlugin> CreatePlugin();
