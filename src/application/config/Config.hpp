#pragma once

#include <nlohmann/json.hpp>
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
    ~Config() = default;

    void LoadConfig();
    void SaveConfig();

    const std::string& GetConfigFilePath() const;
    void SetConfigFilePath(const std::string& path);

  private:
    const json& GetParserConfig(const json& j);
    void ParseXmlConfig(const json& j);
    void GetLoggingConfig(const json& j);

  private:
    std::string m_configFilePath;

  public: // xml config
    std::string xmlRootElement;
    std::string xmlEventElement;
    std::vector<Columns> columns;
    std::string logLevel {"info"}; // Default log level
};

Config& GetConfig();

} // namespace config