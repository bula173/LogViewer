#include "Config.hpp"
#include "BuiltinConversionPlugins.hpp"
//#include "StringReversePlugin.hpp"
#include "Logger.hpp"
#include "KeyEncryption.hpp"
#include <fstream>

#ifdef _WIN32
#include <windows.h>
#endif

#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif

namespace config
{

namespace {
std::filesystem::path GetInstalledEtcDir()
{
#ifdef __APPLE__
    // Resolve: <App>.app/Contents/MacOS/<exe> -> <App>.app/Contents/Resources/etc
    uint32_t size = 0;
    _NSGetExecutablePath(nullptr, &size);
    if (size > 0)
    {
        std::string buffer;
        buffer.resize(size);
        if (_NSGetExecutablePath(buffer.data(), &size) == 0)
        {
            std::error_code ec;
            auto exePath = std::filesystem::weakly_canonical(std::filesystem::path(buffer), ec);
            if (!ec)
            {
                auto resourcesEtc = exePath.parent_path() / ".." / "Resources" / "etc";
                resourcesEtc = std::filesystem::weakly_canonical(resourcesEtc, ec);
                if (!ec && std::filesystem::exists(resourcesEtc))
                    return resourcesEtc;
            }
        }
    }
#endif

    // Fallback: previous behavior (use current working directory)
    return std::filesystem::current_path() / "etc";
}
} // namespace

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
    m_configFilePath = (GetDefaultAppPath() / "config.json").string();
    m_dictionaryFilePath = (GetDefaultAppPath() / "field_dictionary.json").string();
    
    // Register built-in field conversion plugins
    RegisterBuiltinConversionPlugins();
    
    // Register example plugin
    //RegisterStringReversePlugin();
}

void Config::SetAppName(const std::string& name)
{
    appName = name;
};


void Config::LoadConfig()
{
    std::filesystem::path defaultInstalled = GetInstalledEtcDir() / "config.json";

    if (!std::filesystem::exists(m_configFilePath))
    {
        std::filesystem::path configFilePath = GetDefaultConfigPath();
        m_configFilePath = configFilePath.string();
        util::Logger::Info("Using config file path: {}", m_configFilePath);
    }

    if (!std::filesystem::exists(m_configFilePath))
    {
        
        if (!std::filesystem::exists(defaultInstalled))
        {
            util::Logger::Error(
                "Default config template not found in search paths.");
        }

        try
        {
            std::filesystem::copy_file(defaultInstalled, m_configFilePath,
                std::filesystem::copy_options::overwrite_existing);
            util::Logger::Info(
                "Copied default config to user path: {}", m_configFilePath);
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            util::Logger::Error("Failed to copy default config: {}", e.what());
            return; // optional: abort config load
        }
    }
    else
    {
        util::Logger::Info("Config file exists at: {}", m_configFilePath);
        
        // Check if config needs migration (has all new fields)
        try
        {
            std::ifstream userFile(m_configFilePath);
            json userConfig;
            userFile >> userConfig;
            userFile.close();


            if (!defaultInstalled.empty())
            {
                std::ifstream defaultFile(defaultInstalled);
                json defaultConfig;
                defaultFile >> defaultConfig;
                defaultFile.close();

                // Get current application version
                Version::Version currentVersion = Version::current();

                // Check if user config has version
                Version::Version userVersion;
                if (userConfig.contains("version") && userConfig["version"].is_object())
                {
                    userVersion.major = userConfig["version"].value("major", 0);
                    userVersion.minor = userConfig["version"].value("minor", 0);
                    userVersion.patch = userConfig["version"].value("patch", 0);
                    userVersion.type = userConfig["version"].value("type", "");
                }
                else
                {
                    // No version in user config, assume it needs migration
                    userVersion = Version::Version(); // Version 0.0.0
                }

                util::Logger::Info("User config version: {}, Current app version: {}",
                    userVersion.asShortStr(), currentVersion.asShortStr());

                // Only merge if versions differ
                if (userVersion != currentVersion)
                {
                    // Merge configs (user settings take precedence, new fields added)
                    json merged = MergeConfigs(userConfig, defaultConfig);

                    // Add current version to merged config
                    merged["version"] = {
                        {"major", currentVersion.major},
                        {"minor", currentVersion.minor},
                        {"patch", currentVersion.patch},
                        {"type", currentVersion.type}
                    };

                    // Save merged config back
                    std::ofstream outFile(m_configFilePath);
                    outFile << merged.dump(4);
                    outFile.close();
                    util::Logger::Info("Config migrated from {} to {} with new fields",
                        userVersion.asShortStr(), currentVersion.asShortStr());
                }
                else
                {
                    util::Logger::Info("Config version matches application version, no migration needed");
                }
            }
        }
        catch (const std::exception& e)
        {
            util::Logger::Warn("Config migration check failed: {}", e.what());
        }
    }


    // Load the configuration file
    std::ifstream configFile(m_configFilePath);
    if (configFile.is_open())
    {
        json j;
        configFile >> j;
        util::Logger::Info("Loaded config from: {}", m_configFilePath);

        // Check if the JSON data is valid
        if (j.is_null())
        {
            util::Logger::Error("Invalid JSON data in config file.");
            return;
        }

        GetColorConfig(j);
        GetLoggingConfig(j);
        GetFilterConfig(j);
        ParseXmlConfig(j);
        
        // Load config version if present
        if (j.contains("version") && j["version"].is_object())
        {
            m_configVersion.major = j["version"].value("major", 0);
            m_configVersion.minor = j["version"].value("minor", 0);
            m_configVersion.patch = j["version"].value("patch", 0);
            m_configVersion.type = j["version"].value("type", "");
            util::Logger::Info("Loaded config version: {}", m_configVersion.asShortStr());
        }
        else
        {
            // No version in config, assume current version
            m_configVersion = Version::current();
            util::Logger::Info("No version in config, assuming current: {}", m_configVersion.asShortStr());
        }
        
        // Load dictionary file path if specified
        if (j.contains("dictionaryFilePath") && j["dictionaryFilePath"].is_string())
        {
            m_dictionaryFilePath = j["dictionaryFilePath"].get<std::string>();
            util::Logger::Info("Dictionary file path: {}", m_dictionaryFilePath);

            if (!std::filesystem::exists(m_dictionaryFilePath))
            {
                std::filesystem::path defaultDictPath = GetInstalledEtcDir() / "field_dictionary.json";
                util::Logger::Warn("Dictionary file does not exist at specified path: {}", m_dictionaryFilePath);
                util::Logger::Info("Attempting to copy default dictionary from: {}", defaultDictPath.string());
                m_dictionaryFilePath = (GetDefaultAppPath() / "field_dictionary.json").string();
                if (std::filesystem::exists(defaultDictPath))
                {
                    try
                    {
                        std::filesystem::copy_file(defaultDictPath, m_dictionaryFilePath,
                            std::filesystem::copy_options::overwrite_existing);
                        util::Logger::Info(
                            "Copied default dictionary to user path: {}", m_dictionaryFilePath);
                    }
                    catch (const std::filesystem::filesystem_error& e)
                    {
                        util::Logger::Error("Failed to copy default dictionary: {}", e.what());
                    }
                }
            }
            
            // Load dictionary from the specified file
            if (!m_dictionaryFilePath.empty() && std::filesystem::exists(m_dictionaryFilePath))
            {
                util::Logger::Info("Loading field dictionary from: {}", m_dictionaryFilePath);
                m_fieldTranslator.LoadFromFile(m_dictionaryFilePath);

            }
        }


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
        util::Logger::Error("Could not open config file: {}", m_configFilePath);
    }

}

void Config::SetupLogPath()
{
    // Set the log path based on the application name
    std::filesystem::path logPath = GetDefaultLogPath();
    m_logPath = logPath.string();
}

void Config::SaveConfig()
{
    // Save the configuration to the file
    std::ofstream configFile(m_configFilePath);
    if (configFile.is_open())
    {
        json j;

        // Add version information
        j["version"] = {
            {"major", m_configVersion.major},
            {"minor", m_configVersion.minor},
            {"patch", m_configVersion.patch},
            {"type", m_configVersion.type}
        };

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
        j["filters"]["typeFilterField"] = typeFilterField;
        
        // Note: AI configuration is now managed by the AI provider plugin
        // and stored in ~/Library/Application Support/LogViewer/plugins/ai_provider/config.json
        
        // Save dictionary file path
        j["dictionaryFilePath"] = m_dictionaryFilePath;
        
        for (const auto& [col, valMap] : columnColors)
        {
            for (const auto& [val, colors] : valMap)
            {
                j["columnColors"][col][val] = {colors.fg, colors.bg};
            }
        }
        
        // Save item highlights
        for (const auto& [itemKey, highlight] : itemHighlights)
        {
            json highlightJson;
            if (!highlight.backgroundColor.empty())
                highlightJson["backgroundColor"] = highlight.backgroundColor;
            if (!highlight.foregroundColor.empty())
                highlightJson["foregroundColor"] = highlight.foregroundColor;
            if (highlight.bold)
                highlightJson["bold"] = highlight.bold;
            if (highlight.italic)
                highlightJson["italic"] = highlight.italic;
            
            if (!highlightJson.empty())
                j["itemHighlights"][itemKey] = highlightJson;
        }

        configFile << j.dump(4); // Pretty print with 4 spaces
        configFile.close();
        util::Logger::Info("Saved config to: {}", m_configFilePath);
    }
    else
    {
        util::Logger::Error(
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
        util::Logger::Error("Missing 'parsers' in config file.");
        return j; // Return the original JSON if "parsers" is not found
    }
}
void Config::ParseXmlConfig(const json& j)
{

    const json& parser = GetParserConfig(j);

    if (parser.contains("xml"))
    {
        util::Logger::Info("Parsing XML configuration.");
    }
    else
    {
        util::Logger::Warn("Missing 'xml' in config file.");
        return;
    }
    // Check if the XML parser configuration is present
    if (!parser["xml"].is_object())
    {
        util::Logger::Error("Invalid XML parser configuration.");
        return;
    }

    // Set the configuration parameters from the JSON data
    if (parser["xml"].contains("rootElement"))
    {
        xmlRootElement = parser["xml"]["rootElement"].get<std::string>();
    }
    else
    {
        util::Logger::Warn("Missing 'rootElement' in config file.");
    }

    if (parser["xml"].contains("eventElement"))
    {
        xmlEventElement = parser["xml"]["eventElement"].get<std::string>();
    }
    else
    {
        util::Logger::Warn("Missing 'eventElement' in config file.");
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
        util::Logger::Warn("Missing 'columns' in config file.");
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
            util::Logger::Info("Logging level set to: {}", logLevel);
        }
        else
        {
            util::Logger::Warn("Missing 'level' in logging config.");
        }
    }
    else
    {
        util::Logger::Warn("Missing 'logging' in config file.");
    }
}

void Config::GetFilterConfig(const json& j)
{
    if (j.contains("filters"))
    {
        const auto& filterConfig = j["filters"];
        if (filterConfig.contains("typeFilterField"))
        {
            typeFilterField = filterConfig["typeFilterField"].get<std::string>();
            util::Logger::Info("Type filter field set to: {}", typeFilterField);
        }
        else
        {
            util::Logger::Info("No 'typeFilterField' in filter config, using default: type");
        }
    }
    else
    {
        util::Logger::Info("No 'filters' config found, using default type filter field: type");
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
            util::Logger::Error("Failed to create config directory '{}': {}",
                configPath.string(), e.what());
        }
        catch (const std::exception& e)
        {
            util::Logger::Error("Error creating config directory: {}", e.what());
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
#ifdef _WIN32
    // On Windows, append process ID to allow multiple instances
    auto pid = GetCurrentProcessId();
    auto logFileName = "log_" + std::to_string(pid) + ".txt";
#else
    // On Unix systems, file locking is less restrictive
    auto logFileName = "log.txt";
#endif
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
        util::Logger::Warn("Missing 'columnColors' in config file.");
    }
    
    // Load item highlights for ItemDetailsView
    if (j.contains("itemHighlights"))
    {
        for (const auto& item : j["itemHighlights"].items())
        {
            ItemHighlight highlight;
            const auto& config = item.value();
            
            if (config.contains("backgroundColor"))
                highlight.backgroundColor = config["backgroundColor"].get<std::string>();
            if (config.contains("foregroundColor"))
                highlight.foregroundColor = config["foregroundColor"].get<std::string>();
            if (config.contains("bold"))
                highlight.bold = config["bold"].get<bool>();
            if (config.contains("italic"))
                highlight.italic = config["italic"].get<bool>();
            
            itemHighlights[item.key()] = highlight;
        }
        util::Logger::Info("Loaded {} item highlight rules", itemHighlights.size());
    }
}

void Config::Reload()
{
    util::Logger::Info("Reloading configuration from: {}", m_configFilePath);
    LoadConfig();
    util::Logger::Info("Configuration reload complete");
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

void Config::GetPrintConfig() const {

    //iterate over all config parameters and set printConfig accordingly
    util::Logger::Info("Retrieving PrintConfig settings");
    // Load the configuration file
    std::ifstream configFile(m_configFilePath);
    if (configFile.is_open())
    {
        json j;
        configFile >> j;
        util::Logger::Info("Loaded config from: {}", j.dump());
    }
}

const FieldTranslator& Config::GetFieldTranslator() const
{
    return m_fieldTranslator;
}

FieldTranslator& Config::GetMutableFieldTranslator()
{
    return m_fieldTranslator;
}

const std::string& Config::GetDictionaryFilePath() const
{
    return m_dictionaryFilePath;
}

void Config::SetDictionaryFilePath(const std::string& path)
{
    m_dictionaryFilePath = path;
    
    // Reload dictionary from new file
    if (m_fieldTranslator.LoadFromFile(path))
    {
        util::Logger::Info("Reloaded field dictionary from: {}", path);
    }
    else
    {
        util::Logger::Warn("Failed to load field dictionary from: {}", path);
    }
}

json Config::MergeConfigs(const json& userConfig, const json& defaultConfig)
{
    json merged = defaultConfig; // Start with default structure
    
    // Helper to merge columns arrays by name
    auto mergeColumnsArray = [](const json& userCols, const json& defaultCols) -> json {
        if (!userCols.is_array() || !defaultCols.is_array())
            return defaultCols;
        
        json result = json::array();
        std::map<std::string, json> userColMap;
        
        // Build map of user columns by name
        for (const auto& col : userCols)
        {
            if (col.contains("name") && col["name"].is_string())
            {
                userColMap[col["name"]] = col;
            }
        }
        
        // Merge: keep user settings for existing columns, add new columns from default
        for (const auto& defaultCol : defaultCols)
        {
            if (defaultCol.contains("name") && defaultCol["name"].is_string())
            {
                std::string colName = defaultCol["name"];
                if (userColMap.count(colName))
                {
                    // Use user's column settings (preserves visibility, width, etc.)
                    result.push_back(userColMap[colName]);
                    userColMap.erase(colName); // Mark as processed
                }
                else
                {
                    // New column from default config
                    result.push_back(defaultCol);
                }
            }
        }
        
        // Add any user columns that don't exist in defaults (backward compat)
        for (const auto& [name, col] : userColMap)
        {
            result.push_back(col);
        }
        
        return result;
    };
    
    // Recursively merge user values
    std::function<void(json&, const json&, const json&)> deepMerge = 
        [&deepMerge, &mergeColumnsArray](json& target, const json& user, const json& defaults) {
        
        if (!user.is_object() || !defaults.is_object())
        {
            // If user has a value, use it; otherwise keep default
            if (!user.is_null())
                target = user;
            return;
        }
        
        // Merge object fields
        for (auto it = defaults.begin(); it != defaults.end(); ++it)
        {
            const std::string& key = it.key();
            
            // Skip comment fields
            if (key.find("_comment") != std::string::npos)
                continue;
            
            if (user.contains(key))
            {
                // Special handling for columns array
                if (key == "columns" && user[key].is_array() && defaults[key].is_array())
                {
                    target[key] = mergeColumnsArray(user[key], defaults[key]);
                }
                // User has this field - recurse or copy
                else if (user[key].is_object() && defaults[key].is_object())
                {
                    deepMerge(target[key], user[key], defaults[key]);
                }
                else
                {
                    // Use user's value (preserve user settings)
                    target[key] = user[key];
                }
            }
            // else: keep default value (new field added in this version)
        }
        
        // Preserve user fields that don't exist in defaults (for backward compat)
        for (auto it = user.begin(); it != user.end(); ++it)
        {
            const std::string& key = it.key();
            if (!defaults.contains(key))
            {
                target[key] = user[key];
            }
        }
    };
    
    deepMerge(merged, userConfig, defaultConfig);
    return merged;
}

} // namespace config
