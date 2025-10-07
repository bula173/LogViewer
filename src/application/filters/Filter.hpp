#pragma once

#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "db/LogEvent.hpp"

#include <nlohmann/json.hpp>

namespace filters
{

class Filter
{
  public:
    Filter(const std::string& name = "", const std::string& columnName = "",
        const std::string& pattern = "", bool caseSensitive = false,
        bool inverted = false, bool parameterFilter = false,
        const std::string& paramKey = "", int depth = 0);

    // Core properties
    std::string name;
    std::string columnName;
    std::string pattern;
    bool isEnabled = true;
    bool isInverted = false;
    bool isCaseSensitive = false;

    // New fields for complex filtering
    bool isParameterFilter; // Whether this filter works on a parameter instead
                            // of column
    std::string parameterKey; // The parameter key to look for
    int parameterDepth; // How deep to search in nested objects (0 = top level
                        // only)

    // Methods
    bool matches(const std::string& value) const;
    bool matchesParameter(const db::LogEvent& event) const;
    void compile();
    bool searchParameterRecursive(
        const std::vector<std::pair<std::string, std::string>>& items,
        const std::string& key, int currentDepth) const;

    // For serialization to/from JSON
    nlohmann::json toJson() const;
    static Filter fromJson(const nlohmann::json& j);

  private:
    std::shared_ptr<std::regex> regex;
};

using FilterPtr = std::shared_ptr<Filter>;
using FilterList = std::vector<FilterPtr>;

} // namespace filters