#include "config/Config.hpp"
#include <fstream>
#include <spdlog/spdlog.h>

using json = nlohmann::json;

namespace config
{

/**
 * @brief Global function to get the configuration instance.
 * @return Reference to the Config singleton
 */
Config& GetConfig()
{
    return Config::GetInstance();
}

/**
 * @brief Get the singleton instance of the configuration.
 * @return Reference to the Config instance
 */
Config& Config::GetInstance()
{
    static Config instance; // Create static instance inside the method
    return instance;
}

Config::Config()
{
    SetupLogPath();
}

void Config::SetAppName(const std::string& name)
{
    m_appName = name;
};


void Config::LoadConfig()
{
    if (!std::filesystem::exists(m_configFilePath))
    {
        spdlog::warn("Config file not found: {}. Using defaults.",
            m_configFilePath.string());
        return;
    }

    std::ifstream configFile(m_configFilePath);
    if (!configFile.is_open())
    {
        spdlog::error(
            "Could not open config file: {}", m_configFilePath.string());
        return;
    }

    try
    {
        configFile >> m_configData;
        configFile.close();

        // Load all data into local variables
        LoadFromJson();

        spdlog::info(
            "Configuration loaded from: {}", m_configFilePath.string());
    }
    catch (const json::parse_error& e)
    {
        spdlog::error("Invalid JSON data in config file: {}", e.what());
    }
}

void Config::LoadFromJson()
{
    // Load basic settings
    if (m_configData.contains("default_parser"))
        default_parser = m_configData["default_parser"].get<std::string>();

    if (m_configData.contains("logging") &&
        m_configData["logging"].contains("level"))
        logLevel = m_configData["logging"]["level"].get<std::string>();

    // Load XML parser settings
    if (m_configData.contains("parsers") &&
        m_configData["parsers"].contains("xml"))
    {
        const auto& xmlConfig = m_configData["parsers"]["xml"];

        if (xmlConfig.contains("rootElement"))
            xmlRootElement = xmlConfig["rootElement"].get<std::string>();

        if (xmlConfig.contains("eventElement"))
            xmlEventElement = xmlConfig["eventElement"].get<std::string>();

        // Load columns
        if (xmlConfig.contains("columns"))
        {
            columns.clear();
            for (const auto& col : xmlConfig["columns"])
            {
                Column column;
                column.name = col["name"].get<std::string>();
                column.visible = col["visible"].get<bool>();
                column.width = col["width"].get<int>();
                columns.push_back(column);
            }
        }
    }

    // Load column colors
    if (m_configData.contains("columnColors"))
    {
        columnColors.clear();
        for (const auto& [colName, valueMap] :
            m_configData["columnColors"].items())
        {
            ColumnColorMap colorMap;
            for (const auto& [value, colors] : valueMap.items())
            {
                EventTypeColor color;
                color.fg = colors[0].get<std::string>();
                color.bg = colors[1].get<std::string>();
                colorMap[value] = color;
            }
            columnColors[colName] = colorMap;
        }
    }
}

void Config::SetupLogPath()
{
    // Set the log path based on the application name
    std::filesystem::path logPath = GetDefaultLogPath();
    m_logPath = logPath.string();
    spdlog::info("Log file path set to: {}", m_logPath);
}

void Config::SaveConfig()
{
    // Build JSON from local variables
    BuildJsonFromVariables();

    // Save to file
    std::ofstream configFile(m_configFilePath);
    if (configFile.is_open())
    {
        configFile << m_configData.dump(4); // Pretty print with 4 spaces
        configFile.close();
        spdlog::info("Configuration saved to: {}", m_configFilePath.string());
    }
    else
    {
        spdlog::error("Could not open config file for writing: {}",
            m_configFilePath.string());
    }
}

void Config::BuildJsonFromVariables()
{
    m_configData.clear();

    // Basic settings
    m_configData["default_parser"] = default_parser;
    m_configData["logging"]["level"] = logLevel;

    // XML parser settings
    m_configData["parsers"]["xml"]["rootElement"] = xmlRootElement;
    m_configData["parsers"]["xml"]["eventElement"] = xmlEventElement;

    // Columns
    m_configData["parsers"]["xml"]["columns"] = json::array();
    for (const auto& col : columns)
    {
        m_configData["parsers"]["xml"]["columns"].push_back({{"name", col.name},
            {"visible", col.visible}, {"width", col.width}});
    }

    // Column colors
    for (const auto& [colName, valueMap] : columnColors)
    {
        for (const auto& [value, color] : valueMap)
        {
            m_configData["columnColors"][colName][value] = {color.fg, color.bg};
        }
    }
}

void Config::Reload()
{
    LoadConfig();
}

const std::string& Config::GetConfigFilePath() const
{
    return m_configFilePath;
}

void Config::SetConfigFilePath(const std::filesystem::path& path)
{
    m_configFilePath = path;
}

const std::string& Config::GetAppLogPath() const
{
    return m_logPath;
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
        return j; // Return the original JSON if "parsers" is not found
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

void Config::GetLoggingConfig(const json& j)
{
    if (j.contains("logging"))
    {
        const auto& loggingConfig = j["logging"];
        if (loggingConfig.contains("level"))
        {
            logLevel = loggingConfig["level"].get<std::string>();
            spdlog::info("Logging level set to: {}", logLevel);
        }
        else
        {
            spdlog::warn("Missing 'level' in logging config.");
        }
    }
    else
    {
        spdlog::warn("Missing 'logging' in config file.");
    }
}

std::filesystem::path Config::GetDefaultAppPath()
{
    std::filesystem::path configPath;
    std::string appName = "LogViewer"; // Default app name

#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData)
    {
        configPath = std::filesystem::path(appData) / appName;
    }
#elif __APPLE__
    const char* home = std::getenv("HOME");
    if (home)
    {
        configPath = std::filesystem::path(home) / "Library" /
            "Application Support" / appName;
    }
#else // Linux and others
    const char* configHome = std::getenv("XDG_CONFIG_HOME");
    const char* home = std::getenv("HOME");
    if (configHome)
    {
        configPath = std::filesystem::path(configHome) / appName;
    }
    else if (home)
    {
        configPath = std::filesystem::path(home) / ".config" / appName;
    }
#endif

    if (!configPath.empty())
    {
        try
        {
            std::filesystem::create_directories(configPath);
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            spdlog::error("Failed to create config directory '{}': {}",
                configPath.string(), e.what());
        }
        catch (const std::exception& e)
        {
            spdlog::error("Error creating config directory: {}", e.what());
        }
    }

    return configPath;
}

std::filesystem::path Config::GetDefaultConfigPath()
{
    auto configFileName = "config.json";
    auto configFilePath = GetDefaultAppPath() / configFileName;
    return configFilePath;
}

std::filesystem::path Config::GetDefaultLogPath()
{
    auto logFileName = "log.txt";
    auto logFilePath = GetDefaultAppPath() / logFileName;
    return logFilePath;
}

} // namespace config
