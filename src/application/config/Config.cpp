#include "config/Config.hpp"
#include "util/Logger.hpp"
#include "util/KeyEncryption.hpp"
#include <fstream>

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


void Config::LoadConfig()
{

    if (!std::filesystem::exists(m_configFilePath))
    {
        std::filesystem::path configFilePath = GetDefaultConfigPath();
        m_configFilePath = configFilePath.string();
        util::Logger::Info("Using config file path: {}", m_configFilePath);
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
                util::Logger::Info("Default config template found at: {}",
                    defaultInstalled.string());
                break;
            }
        }

        if (!std::filesystem::exists(defaultInstalled))
        {
            util::Logger::Error(
                "Default config template not found in search paths. Aborting.");
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
            
            // Load default config template
            std::filesystem::path cwd = std::filesystem::current_path();
            std::vector<std::filesystem::path> searchPaths = {
                cwd / "etc" / "default_config.json",
                cwd / "config" / "default_config.json",
                cwd / "default_config.json"};
            
            std::filesystem::path defaultPath;
            for (const auto& path : searchPaths)
            {
                if (std::filesystem::exists(path))
                {
                    defaultPath = path;
                    break;
                }
            }
            
            if (!defaultPath.empty())
            {
                std::ifstream defaultFile(defaultPath);
                json defaultConfig;
                defaultFile >> defaultConfig;
                defaultFile.close();
                
                // Merge configs (user settings take precedence, new fields added)
                json merged = MergeConfigs(userConfig, defaultConfig);
                
                // Save merged config back if it changed
                if (merged != userConfig)
                {
                    std::ofstream outFile(m_configFilePath);
                    outFile << merged.dump(4);
                    outFile.close();
                    util::Logger::Info("Config migrated with new fields from default template");
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
        GetAIConfig(j);
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
        j["aiConfig"]["provider"] = aiProvider;
        
        // Save provider-specific API keys (encrypted)
        if (!openaiApiKey.empty())
        {
            std::string keyToSave = openaiApiKey;
            if (!util::KeyEncryption::IsEncrypted(keyToSave))
            {
                keyToSave = util::KeyEncryption::Encrypt(keyToSave);
            }
            j["aiConfig"]["openaiApiKey"] = keyToSave;
        }
        if (!anthropicApiKey.empty())
        {
            std::string keyToSave = anthropicApiKey;
            if (!util::KeyEncryption::IsEncrypted(keyToSave))
            {
                keyToSave = util::KeyEncryption::Encrypt(keyToSave);
            }
            j["aiConfig"]["anthropicApiKey"] = keyToSave;
        }
        if (!googleApiKey.empty())
        {
            std::string keyToSave = googleApiKey;
            if (!util::KeyEncryption::IsEncrypted(keyToSave))
            {
                keyToSave = util::KeyEncryption::Encrypt(keyToSave);
            }
            j["aiConfig"]["googleApiKey"] = keyToSave;
        }
        if (!xaiApiKey.empty())
        {
            std::string keyToSave = xaiApiKey;
            if (!util::KeyEncryption::IsEncrypted(keyToSave))
            {
                keyToSave = util::KeyEncryption::Encrypt(keyToSave);
            }
            j["aiConfig"]["xaiApiKey"] = keyToSave;
        }
        
        j["aiConfig"]["baseUrl"] = ollamaBaseUrl;
        j["aiConfig"]["defaultModel"] = ollamaDefaultModel;
        j["aiConfig"]["timeoutSeconds"] = aiTimeoutSeconds;
        for (const auto& [col, valMap] : columnColors)
        {
            for (const auto& [val, colors] : valMap)
            {
                j["columnColors"][col][val] = {colors.fg, colors.bg};
            }
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

void Config::GetAIConfig(const json& j)
{
    if (j.contains("aiConfig"))
    {
        const auto& aiConfig = j["aiConfig"];
        if (aiConfig.contains("provider"))
        {
            aiProvider = aiConfig["provider"].get<std::string>();
            util::Logger::Info("AI provider set to: {}", aiProvider);
        }
        // Load provider-specific API keys (backward compatibility with old "apiKey" field)
        if (aiConfig.contains("apiKey"))
        {
            const std::string storedKey = aiConfig["apiKey"].get<std::string>();
            // Decrypt old generic key and migrate based on provider
            const std::string decryptedKey = util::KeyEncryption::Decrypt(storedKey);
            if (aiProvider == "openai") openaiApiKey = decryptedKey;
            else if (aiProvider == "anthropic") anthropicApiKey = decryptedKey;
            else if (aiProvider == "google") googleApiKey = decryptedKey;
            else if (aiProvider == "xai") xaiApiKey = decryptedKey;
            util::Logger::Info("Migrated legacy API key for provider: {}", aiProvider);
        }
        
        // Load new provider-specific keys
        if (aiConfig.contains("openaiApiKey"))
        {
            const std::string storedKey = aiConfig["openaiApiKey"].get<std::string>();
            openaiApiKey = util::KeyEncryption::Decrypt(storedKey);
            util::Logger::Info("OpenAI API key loaded (encrypted: {})", 
                             util::KeyEncryption::IsEncrypted(storedKey));
        }
        if (aiConfig.contains("anthropicApiKey"))
        {
            const std::string storedKey = aiConfig["anthropicApiKey"].get<std::string>();
            anthropicApiKey = util::KeyEncryption::Decrypt(storedKey);
            util::Logger::Info("Anthropic API key loaded (encrypted: {})", 
                             util::KeyEncryption::IsEncrypted(storedKey));
        }
        if (aiConfig.contains("googleApiKey"))
        {
            const std::string storedKey = aiConfig["googleApiKey"].get<std::string>();
            googleApiKey = util::KeyEncryption::Decrypt(storedKey);
            util::Logger::Info("Google API key loaded (encrypted: {})", 
                             util::KeyEncryption::IsEncrypted(storedKey));
        }
        if (aiConfig.contains("xaiApiKey"))
        {
            const std::string storedKey = aiConfig["xaiApiKey"].get<std::string>();
            xaiApiKey = util::KeyEncryption::Decrypt(storedKey);
            util::Logger::Info("xAI API key loaded (encrypted: {})", 
                             util::KeyEncryption::IsEncrypted(storedKey));
        }
        if (aiConfig.contains("baseUrl"))
        {
            ollamaBaseUrl = aiConfig["baseUrl"].get<std::string>();
            util::Logger::Info("AI base URL set to: {}", ollamaBaseUrl);
        }
        if (aiConfig.contains("defaultModel"))
        {
            ollamaDefaultModel = aiConfig["defaultModel"].get<std::string>();
            util::Logger::Info("AI default model set to: {}", ollamaDefaultModel);
        }
        if (aiConfig.contains("timeoutSeconds"))
        {
            aiTimeoutSeconds = aiConfig["timeoutSeconds"].get<int>();
            util::Logger::Info("AI timeout set to: {} seconds", aiTimeoutSeconds);
        }
    }
    else
    {
        util::Logger::Info("No AI config found, using defaults.");
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
        util::Logger::Warn("Missing 'columnColors' in config file.");
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

std::string Config::GetApiKeyForProvider(const std::string& provider) const
{
    if (provider == "openai")
        return openaiApiKey;
    else if (provider == "anthropic")
        return anthropicApiKey;
    else if (provider == "google")
        return googleApiKey;
    else if (provider == "xai")
        return xaiApiKey;
    else
        return ""; // Local providers don't need API keys
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
