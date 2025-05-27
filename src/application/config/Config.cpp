#include "config/Config.hpp"
#include <fstream>
#include <spdlog/spdlog.h>

namespace config
{

static Config configInstance;
// This is a singleton instance of the Config class
// that is used to manage the configuration settings.
// It is defined in the cpp file to ensure that there is only one instance
// of the Config class throughout the application.
// This is a common pattern in C++ to provide a global access point
// to a configuration object without exposing the details of its implementation.
// The instance is created statically, and the GetConfig function
// provides access to this instance.
// This allows other parts of the application to access the configuration
// settings without needing to create their own instances of the Config class.
// The instance is created statically, and the GetConfig function
// provides access to this instance.
Config& GetConfig()
{
    return configInstance;
}

void Config::LoadConfig()
{
    // Load the configuration file
    std::ifstream configFile(m_configFilePath);
    if (configFile.is_open())
    {
        json j;
        configFile >> j;
        spdlog::info("Loaded config from: {}", m_configFilePath);

        // Check if the JSON data is valid
        if (j.is_null())
        {
            spdlog::error("Invalid JSON data in config file.");
            return;
        }

        ParseXmlConfig(j);

        // Example: m_someParameter = j["someParameter"].get<std::string>();
        configFile.close();
    }
    else
    {
        spdlog::error("Could not open config file: {}", m_configFilePath);
    }
}

void Config::SaveConfig()
{
    // Save the configuration to the file
    std::ofstream configFile(m_configFilePath);
    if (configFile.is_open())
    {
        json j;

        // Set the JSON data from the configuration parameters
        // Example: j["someParameter"] = m_someParameter;

        configFile << j.dump(4); // Pretty print with 4 spaces
        configFile.close();
        spdlog::info("Saved config to: {}", m_configFilePath);
    }
    else
    {
        spdlog::error(
            "Could not open config file for writing: {}", m_configFilePath);
    }
}

const std::string& Config::GetConfigFilePath() const
{
    return m_configFilePath;
}

void Config::SetConfigFilePath(const std::string& path)
{
    m_configFilePath = path;
}

const json& Config::GetParserConfig(const json& j)
{
    if (j.contains("parsers"))
    {
        return j["parsers"];
    }
    else
    {
        spdlog::error("Missing 'parsers' in config file.");
    }
}
void Config::ParseXmlConfig(const json& j)
{

    const json& parser = GetParserConfig(j);

    if (parser.contains("xml"))
    {
        spdlog::info("Parsing XML configuration.");
    }
    else
    {
        spdlog::warn("Missing 'xml' in config file.");
        return;
    }
    // Check if the XML parser configuration is present
    if (!parser["xml"].is_object())
    {
        spdlog::error("Invalid XML parser configuration.");
        return;
    }

    // Set the configuration parameters from the JSON data
    if (parser["xml"].contains("rootElement"))
    {
        xmlRootElement = parser["xml"]["rootElement"].get<std::string>();
    }
    else
    {
        spdlog::warn("Missing 'rootElement' in config file.");
    }

    if (parser["xml"].contains("eventElement"))
    {
        xmlEventElement = parser["xml"]["eventElement"].get<std::string>();
    }
    else
    {
        spdlog::warn("Missing 'eventElement' in config file.");
    }

    if (parser["xml"].contains("columns"))
    {
        for (const auto& col : parser["xml"]["columns"])
        {
            Columns column;
            column.name = col["name"].get<std::string>();
            column.visible = col.value("visible", true);
            column.width = col.value("width", 100); // Default width
            columns.push_back(column);
        }
    }
    else
    {
        spdlog::warn("Missing 'columns' in config file.");
    }
}

} // namespace config
