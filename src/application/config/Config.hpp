#pragma once

#include <nlohmann/json.hpp>
// std
#include <filesystem>
#include <string>


namespace config
{

using json = nlohmann::json;

struct Columns
{
    std::string name;
    bool visible {true};
    int width {100}; // Default width
};

class Config
{
  public:
    Config() = default;
    Config(const Config&) = delete;
    Config& operator=(const Config&) = delete;
    Config(Config&&) = delete;
    Config& operator=(Config&&) = delete;
    virtual ~Config() = default;

    void LoadConfig();
    void SaveConfig();

    const std::string& GetConfigFilePath() const;
    const std::string& GetAppLogPath() const;
    void SetConfigFilePath(const std::string& path);
    void SetAppName(const std::string& name);

  private:
    const json& GetParserConfig(const json& j);
    void ParseXmlConfig(const json& j);
    void GetLoggingConfig(const json& j);
    std::filesystem::path GetDefaultConfigPath();
    std::filesystem::path GetDefaultLogPath();
    std::filesystem::path GetDefaultAppPath();
    void SetupLogPath();

  private:
    std::string m_configFilePath {"etc/config.json"}; // Default path
    std::string m_logPath {"log.txt"};                // Default log file path

  public: // xml config
    std::string appName {"LogViewer"};
    std::string xmlRootElement;
    std::string xmlEventElement;
    std::vector<Columns> columns;
    std::string logLevel {"info"}; // Default log level
};

Config& GetConfig();

} // namespace config