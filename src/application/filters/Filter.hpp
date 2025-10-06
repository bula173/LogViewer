#pragma once

#include <memory>
#include <regex>
#include <string>
#include <vector>

#include <nlohmann/json.hpp>

namespace filters
{

class Filter
{
  public:
    Filter() = default;
    Filter(const std::string& name, const std::string& columnName,
        const std::string& pattern, bool caseSensitive = false,
        bool inverted = false);

    // Core properties
    std::string name;
    std::string columnName;
    std::string pattern;
    bool isEnabled = true;
    bool isInverted = false;
    bool isCaseSensitive = false;

    // Methods
    bool matches(const std::string& value) const;
    void compile();

    // For serialization to/from JSON
    nlohmann::json toJson() const;
    static Filter fromJson(const nlohmann::json& j);

  private:
    std::shared_ptr<std::regex> regex;
};

using FilterPtr = std::shared_ptr<Filter>;
using FilterList = std::vector<FilterPtr>;

} // namespace filters