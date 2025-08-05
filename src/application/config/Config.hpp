#pragma once
#include <filesystem>
#include <map>
#include <nlohmann/json.hpp>
#include <string>
#include <vector>


namespace config
{
using json = nlohmann::json;

/**
 * @brief Represents a column configuration for the data view.
 */
struct Column
{
    std::string name; ///< Column name/identifier
    bool visible;     ///< Whether the column is visible
    int width;        ///< Column width in pixels
};

/**
 * @brief Color configuration for event types.
 */
struct EventTypeColor
{
    std::string fg; ///< Font color (e.g., "#ffffff")
    std::string bg; ///< Background color (e.g., "#000000")
};

/// Map of value to color configuration (e.g., "alarm" -> color)
using ColumnColorMap = std::map<std::string, EventTypeColor>;
/// Map of column name to value-color mappings (e.g., "type" -> (value ->
/// color))
using AllColumnColors = std::map<std::string, ColumnColorMap>;

/**
 * @brief Main configuration class that manages application settings.
 *
 * This class follows the singleton pattern and provides centralized
 * access to all configuration parameters. All settings are stored
 * in local variables as the single source of truth.
 */
class Config
{
  public:
    // Configuration variables - single source of truth
    std::string default_parser;   ///< Default parser to use (e.g., "xml")
    std::string logLevel;         ///< Logging level (e.g., "debug", "info")
    std::string xmlRootElement;   ///< XML root element name
    std::string xmlEventElement;  ///< XML event element name
    std::vector<Column> columns;  ///< Column configurations
    AllColumnColors columnColors; ///< Color mappings for event types

    /**
     * @brief Get the singleton instance of the configuration.
     * @return Reference to the Config instance
     */
    static Config& GetInstance();

    /**
     * @brief Load configuration from file into local variables.
     *
     * Reads the JSON configuration file and populates all local
     * variables. If file doesn't exist or is invalid, uses defaults.
     */
    void LoadConfig();

    /**
     * @brief Save current configuration to file.
     *
     * Builds JSON from current local variables and writes to file.
     */
    void SaveConfig();

    /**
     * @brief Reload configuration from file.
     *
     * Convenience method that calls LoadConfig().
     */
    void Reload();

    /**
     * @brief Get the default configuration file path.
     * @return Path to the default config file
     */
    static std::filesystem::path GetDefaultConfigPath();

    /**
     * @brief Get the default log file path.
     * @return Path to the default log file
     */
    static std::filesystem::path GetDefaultLogPath();

    /**
     * @brief Set the configuration file path (mainly for testing).
     * @param path Path to the configuration file
     */
    void SetConfigFilePath(const std::filesystem::path& path);
    /**
     * @brief Set the application name for configuration paths.
     * @param name Name of the application
     */
    void SetAppName(const std::string& name);

  private:
    Config(); ///< Private constructor for singleton pattern

    std::filesystem::path m_configFilePath; ///< Path to the configuration file
    nlohmann::json m_configData; ///< JSON data (only used during load/save)
    std::string m_appName;       ///< Application name for config paths
    std::string m_logPath;       ///< Path to the log file
    /**
     * @brief Load data from JSON into local variables.
     *
     * Internal method that parses m_configData and populates
     * all configuration variables.
     */
    void LoadFromJson();

    /**
     * @brief Build JSON from current local variables.
     *
     * Internal method that constructs m_configData from
     * current configuration variables before saving.
     */
    void BuildJsonFromVariables();

    /**
     * @brief Get the default application path for configuration files.
     * @return Default path for config files
     */
    static std::filesystem::path GetDefaultAppPath();

    /**
     * @brief Set up the log file path.
     */
    void SetupLogPath();
};

/**
 * @brief Global function to get the configuration instance.
 * @return Reference to the Config singleton
 */
Config& GetConfig();
}