#pragma once

#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "db/LogEvent.hpp"
#include "filters/IFilterStrategy.hpp"

#include <nlohmann/json.hpp>

namespace filters
{

/**
 * @class Filter
 * @brief Filter for log events with pluggable matching strategies
 *
 * Uses Strategy pattern for flexible matching algorithms:
 * - Regex (default)
 * - Exact match
 * - Fuzzy match
 * - Wildcard match
 *
 * @par Thread Safety:
 * Filter instances should not be modified during concurrent matching.
 * Strategy instances are immutable and thread-safe.
 */
class Filter
{
  public:
    Filter(const std::string& name = "", const std::string& columnName = "",
        const std::string& pattern = "", bool caseSensitive = false,
        bool inverted = false, bool parameterFilter = false,
        const std::string& paramKey = "", int depth = 0);

    // Copy constructor and assignment - need custom implementation because of unique_ptr
    Filter(const Filter& other);
    Filter& operator=(const Filter& other);
    
    // Move operations (default is fine)
    Filter(Filter&&) = default;
    Filter& operator=(Filter&&) = default;

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

    /**
     * @brief Set matching strategy
     * @param strategy New strategy to use (takes ownership)
     *
     * Allows runtime switching between matching algorithms.
     * Default is RegexFilterStrategy.
     */
    void setStrategy(std::unique_ptr<IFilterStrategy> strategy);

    /**
     * @brief Get current strategy name
     * @return Strategy identifier (e.g., "regex", "exact")
     */
    std::string getStrategyName() const;

    // For serialization to/from JSON
    nlohmann::json toJson() const;
    static Filter fromJson(const nlohmann::json& j);

  private:
    std::shared_ptr<std::regex> regex; // Legacy regex support
    std::unique_ptr<IFilterStrategy> m_strategy; // Strategy pattern
};

using FilterPtr = std::shared_ptr<Filter>;
using FilterList = std::vector<FilterPtr>;

} // namespace filters