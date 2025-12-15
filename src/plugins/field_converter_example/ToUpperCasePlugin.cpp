#include "plugins/IPlugin.hpp"
#include "plugins/IFieldConversionPlugin.hpp"
#include <algorithm>
#include <cctype>

namespace plugin
{

/**
 * @brief Field conversion plugin that converts text to uppercase
 * This plugin implements both IPlugin and IFieldConversionPlugin interfaces
 */
class ToUpperCasePlugin : public IPlugin, public IFieldConversionPlugin
{
public:
    ToUpperCasePlugin()
        : m_status(PluginStatus::Unloaded)
        , m_lastError("")
    {
    }

    // IFieldConversionPlugin interface
    std::string GetConversionType() const override
    {
        return "to_uppercase";
    }

    std::string GetDescription() const override
    {
        return "Convert text to uppercase letters";
    }

    std::string Convert(std::string_view value, const std::map<std::string, std::string>& /*parameters*/) const override
    {
        std::string result(value);
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return std::toupper(c); });
        return result;
    }

    std::string GetConversionName() const override
    {
        return "To Uppercase";
    }

    // IPlugin interface implementation
    PluginMetadata GetMetadata() const override
    {
        return PluginMetadata{
            "field_converter_to_uppercase",  // id
            "To Uppercase Field Converter",  // name
            "1.0.0",                        // version
            "1.0.0",                        // apiVersion
            "LogViewer Team",               // author
            "Converts text field values to uppercase", // description
            "",                             // website
            PluginType::FieldConversion,    // type
            false,                          // requiresLicense
            {}                              // dependencies
        };
    }

    bool Initialize() override
    {
        try {
            m_status = PluginStatus::Initialized;
            return true;
        }
        catch (const std::exception& ex) {
            m_lastError = std::string("Initialization failed: ") + ex.what();
            m_status = PluginStatus::Error;
            return false;
        }
    }

    void Shutdown() override
    {
        m_status = PluginStatus::Unloaded;
    }

    PluginStatus GetStatus() const override
    {
        return m_status;
    }

    std::string GetLastError() const override
    {
        return m_lastError;
    }

    bool IsLicensed() const override
    {
        return true; // No license required
    }

    bool SetLicense(const std::string& /*licenseKey*/) override
    {
        return true; // No license required
    }

    IFieldConversionPlugin* GetFieldConversionInterface() override
    {
        return this; // Return this plugin as the field conversion implementation
    }

    bool ValidateConfiguration() const override
    {
        return true; // No configuration needed
    }

private:
    PluginStatus m_status;
    std::string m_lastError;
};

} // namespace plugin

// Export C function for plugin loading
extern "C" {
EXPORT_PLUGIN(plugin::ToUpperCasePlugin)
}