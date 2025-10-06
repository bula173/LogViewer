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

Config::Config()
{
    SetupLogPath();
}

void Config::SetAppName(const std::string& name)
{
    appName = name;
};


bool Config::LoadConfig()
{

    if (!std::filesystem::exists(m_configFilePath))
    {
        std::filesystem::path configFilePath = GetDefaultConfigPath();
        m_configFilePath = configFilePath.string();
        spdlog::info("Using config file path: {}", m_configFilePath);
    }

    if (!std::filesystem::exists(m_configFilePath))
    {
        std::filesystem::path cwd = std::filesystem::current_path();
        std::filesystem::path defaultInstalled = cwd / "default_config.json";

        std::vector<std::filesystem::path> searchPaths = {
            cwd / "etc" / "default_config.json",
            cwd / "config" / "default_config.json",
            cwd / "default_config.json"};

        for (const auto& path : searchPaths)
        {
            if (std::filesystem::exists(path))
            {
                defaultInstalled = path;
                spdlog::info("Default config template found at: {}",
                    defaultInstalled.string());
                break;
            }
        }

        if (!std::filesystem::exists(defaultInstalled))
        {
            spdlog::error(
                "Default config template not found in search paths. Aborting.");
        }

        try
        {
            std::filesystem::copy_file(defaultInstalled, m_configFilePath,
                std::filesystem::copy_options::overwrite_existing);
            spdlog::info(
                "Copied default config to user path: {}", m_configFilePath);
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            spdlog::error("Failed to copy default config: {}", e.what());
            return; // optional: abort config load
        }
    }
    else
    {
        spdlog::info("Config file exists at: {}", m_configFilePath);
    }


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

        GetColorConfig(j);
        GetLoggingConfig(j);
        ParseXmlConfig(j);


        // Example: m_someParameter = j["someParameter"].get<std::string>();
        for (auto& [col, valMap] : j["columnColors"].items())
        {
            for (auto& [val, colors] : valMap.items())
            {
                columnColors[col][val] = {colors[0], colors[1]};
            }
        }

        configFile.close();
    }
    else
    {
        spdlog::error("Could not open config file: {}", m_configFilePath);
    }

    return true; // Config loaded successfully
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
    // Save the configuration to the file
    std::ofstream configFile(m_configFilePath);
    if (configFile.is_open())
    {
        json j;

        // Set the JSON data from the configuration parameters
        // Example: j["someParameter"] = m_someParameter;
        j["parsers"]["xml"]["rootElement"] = xmlRootElement;
        j["parsers"]["xml"]["eventElement"] = xmlEventElement;
        j["parsers"]["xml"]["columns"] = json::array();
        for (const auto& col : columns)
        {
            j["parsers"]["xml"]["columns"].push_back({{"name", col.name},
                {"visible", col.isVisible}, {"width", col.width}});
        }
        j["logging"]["level"] = logLevel;
        for (const auto& [col, valMap] : columnColors)
        {
            for (const auto& [val, colors] : valMap)
            {
                j["columnColors"][col][val] = {colors.fg, colors.bg};
            }
        }

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
            ColumnConfig column;
            column.name = col["name"].get<std::string>();
            column.isVisible = col.value("visible", true);
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
    auto cofigFilePath = GetDefaultAppPath() / configFileName;
    return cofigFilePath;
}

std::filesystem::path Config::GetDefaultLogPath()
{
    auto logFileName = "log.txt";
    auto logFilePath = GetDefaultAppPath() / logFileName;
    return logFilePath;
}

void Config::GetColorConfig(const json& j)
{
    if (j.contains("columnColors"))
    {
        for (const auto& col : j["columnColors"].items())
        {
            ValueColorMap valueColorMap;
            for (const auto& val : col.value().items())
            {
                valueColorMap[val.key()] = {val.value()[0], val.value()[1]};
            }
            columnColors[col.key()] = std::move(valueColorMap);
        }
    }
    else
    {
        spdlog::warn("Missing 'columnColors' in config file.");
    }
}

void Config::Reload()
{
    spdlog::info("Reloading configuration from: {}", m_configFilePath);
    LoadConfig();
    spdlog::info("Configuration reload complete");
}

// Read-only access for UI display
const std::vector<ColumnConfig>& Config::GetColumns() const
{
    return columns;
}

// Mutable access for configuration
std::vector<ColumnConfig>& Config::GetMutableColumns()
{
    return columns;
}

} // namespace config
