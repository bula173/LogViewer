#pragma once

#include "FieldTranslator.hpp"
#include "Version.hpp"
#include <nlohmann/json.hpp>
// std
#include <filesystem>
#include <map>
#include <string>
#include <vector>


namespace config
{

using json = nlohmann::json;

struct ColumnConfig
{
    std::string name;
    bool isVisible {true};
    int width {100}; // Default width
};

struct ColumnColor
{
    std::string fg;
    std::string bg;
};
using ValueColorMap = std::map<std::string, ColumnColor>; // value -> color
using ColumnColorMap =
    std::map<std::string, ValueColorMap>; // column -> (value -> color)

struct ItemHighlight
{
    std::string backgroundColor;
    std::string foregroundColor;
    bool bold {false};
    bool italic {false};
};
using ItemHighlightMap = std::map<std::string, ItemHighlight>; // item key -> highlight style

class Config
{
  public:
    Config();
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(Config&&) = delete;
    virtual ~Config() = default;

    void LoadConfig();
    void SaveConfig();

    /**
     * @brief Reload configuration from file.
     *
     * Convenience method that calls LoadConfig() to reload
     * the configuration from the current config file path.
     */
    void Reload();

    const std::string& GetConfigFilePath() const;
    const std::string& GetAppLogPath() const;
    void SetConfigFilePath(const std::string& path);
    void SetAppName(const std::string& name);
    // Read-only access for UI display
    const std::vector<ColumnConfig>& GetColumns() const;
    // Mutable access for configuration
    std::vector<ColumnConfig>& GetMutableColumns();
    void GetPrintConfig() const;
    
    /**
     * @brief Get the field translator for value conversions
     * @return Reference to the FieldTranslator instance
     */
    const FieldTranslator& GetFieldTranslator() const;
    
    /**
     * @brief Get mutable field translator for editing dictionary
     * @return Mutable reference to the FieldTranslator instance
     */
    FieldTranslator& GetMutableFieldTranslator();
    
    /**
     * @brief Get the dictionary file path
     * @return Path to the field dictionary file
     */
    const std::string& GetDictionaryFilePath() const;
    
    /**
     * @brief Set the dictionary file path and reload dictionary
     * @param path Path to the field dictionary file
     */
    void SetDictionaryFilePath(const std::string& path);

    const std::string& GetAppName() const { return appName; }

    /**
     * @brief Get the current config version
     * @return The version of the config file
     */
    const Version::Version& GetConfigVersion() const { return m_configVersion; }

    /**
     * @brief Set the config version
     * @param version The version to set
     */
    void SetConfigVersion(const Version::Version& version) { m_configVersion = version; }

    /**
     * @brief Get the default application path
     * @return Path to the default application directory
     */
    std::filesystem::path GetDefaultAppPath();

  private:
    const json& GetParserConfig(const json& j);
    void ParseXmlConfig(const json& j);
    void GetLoggingConfig(const json& j);
    void GetFilterConfig(const json& j);
    std::filesystem::path GetDefaultConfigPath();
    std::filesystem::path GetDefaultLogPath();
    void SetupLogPath();
    void GetColorConfig(const json& j);
    
    /**
     * @brief Merge new fields from default config into user config
     * @param userConfig User's existing config
     * @param defaultConfig Default config from application
     * @return Merged config with user values preserved and new fields added
     */
    json MergeConfigs(const json& userConfig, const json& defaultConfig);

  private:
    std::string m_configFilePath {"config.json"}; // Default path
    std::string m_logPath {"log.txt"};                // Default log file path
    std::string m_dictionaryFilePath {"field_dictionary.json"}; // Dictionary config path
    FieldTranslator m_fieldTranslator;                // Field value translator
    Version::Version m_configVersion;                 // Version of the config file

  public: // xml config
    std::string appName {"LogViewer"};
    std::string xmlRootElement;
    std::string xmlEventElement;
    std::vector<ColumnConfig> columns;
    std::string logLevel {"debug"}; // Default log level
    ColumnColorMap columnColors;
    ItemHighlightMap itemHighlights;  // Item details view highlighting by key name
    
    // Filter configuration
    std::string typeFilterField {"type"};  // Field to use for type filtering (e.g., "type", "level", "severity")

    // Update settings
    struct UpdateSettings {
        bool        checkOnStartup    {true};
        int         checkIntervalDays {7};
        std::string lastCheckTime     {};  // ISO 8601 UTC, e.g. "2026-04-25T10:00:00Z"
    } updates;
};

Config& GetConfig();

} // namespace config