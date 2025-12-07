#pragma once

#include "config/FieldTranslator.hpp"
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
     * @brief Get the API key for a specific provider
     * @param provider The provider name ("openai", "anthropic", "google", "xai")
     * @return The API key for that provider, or empty string if not set
     */
    std::string GetApiKeyForProvider(const std::string& provider) const;
    
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

  private:
    const json& GetParserConfig(const json& j);
    void ParseXmlConfig(const json& j);
    void GetLoggingConfig(const json& j);
    void GetFilterConfig(const json& j);
    void GetAIConfig(const json& j);
    std::filesystem::path GetDefaultConfigPath();
    std::filesystem::path GetDefaultLogPath();
    std::filesystem::path GetDefaultAppPath();
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
    std::string m_configFilePath {"etc/config.json"}; // Default path
    std::string m_logPath {"log.txt"};                // Default log file path
    std::string m_dictionaryFilePath {"etc/field_dictionary.json"}; // Dictionary config path
    FieldTranslator m_fieldTranslator;                // Field value translator

  public: // xml config
    std::string appName {"LogViewer"};
    std::string xmlRootElement;
    std::string xmlEventElement;
    std::vector<ColumnConfig> columns;
    std::string logLevel {"debug"}; // Default log level
    ColumnColorMap columnColors;
    
    // Filter configuration
    std::string typeFilterField {"type"};  // Field to use for type filtering (e.g., "type", "level", "severity")
    
    // AI configuration
    std::string aiProvider {"ollama"};  // "ollama", "lmstudio", "openai", "anthropic", "google", "xai"
    
    // API keys for cloud providers (encrypted in config file)
    std::string openaiApiKey;
    std::string anthropicApiKey;
    std::string googleApiKey;
    std::string xaiApiKey;
    
    std::string ollamaBaseUrl {"http://localhost:11434"};
    std::string ollamaDefaultModel {"qwen2.5-coder:7b"};
    int aiTimeoutSeconds {300};  // Timeout for AI requests in seconds (default 5 minutes)
};

Config& GetConfig();

} // namespace config