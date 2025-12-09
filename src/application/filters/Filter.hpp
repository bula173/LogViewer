#pragma once

#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "db/LogEvent.hpp"
#include "filters/IFilterStrategy.hpp"
#include "mvc/IModel.hpp"

#include <nlohmann/json.hpp>

namespace filters
{

/**
 * @struct FilterCondition
 * @brief Represents a single condition within a filter
 *
 * A filter can have multiple conditions that are combined with AND logic.
 */
struct FilterCondition
{
    std::string columnName;
    std::string pattern;
    bool isParameterFilter = false;
    std::string parameterKey;
    int parameterDepth = 0;
    bool isCaseSensitive = false;
    std::unique_ptr<IFilterStrategy> strategy;

    FilterCondition() = default;
    FilterCondition(const std::string& col, const std::string& pat,
                   bool paramFilter = false, const std::string& paramKey = "",
                   int depth = 0, bool caseSensitive = false)
        : columnName(col), pattern(pat), isParameterFilter(paramFilter),
          parameterKey(paramKey), parameterDepth(depth), isCaseSensitive(caseSensitive)
    {
        strategy = std::make_unique<RegexFilterStrategy>();
    }

    FilterCondition(const FilterCondition& other);
    FilterCondition& operator=(const FilterCondition& other);
    FilterCondition(FilterCondition&&) = default;
    FilterCondition& operator=(FilterCondition&&) = default;

    bool matches(const db::LogEvent& event) const;
    nlohmann::json toJson() const;
    static FilterCondition fromJson(const nlohmann::json& j);

  private:
    bool searchParameterRecursive(
        const std::vector<std::pair<std::string, std::string>>& items,
        const std::string& key, int currentDepth) const;
};

/**
 * @class Filter
 * @brief Filter for log events with pluggable matching strategies
 *
 * Supports multiple conditions combined with AND logic.
 * Uses Strategy pattern for flexible matching algorithms.
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

    // Core properties (kept for backward compatibility)
    std::string name;
    std::string columnName;  // Primary column for single-condition filters
    std::string pattern;     // Primary pattern for single-condition filters
    bool isEnabled = true;
    bool isInverted = false;
    bool isCaseSensitive = false;

    // New fields for complex filtering
    bool isParameterFilter; // Whether this filter works on a parameter instead
                            // of column
    std::string parameterKey; // The parameter key to look for
    int parameterDepth; // How deep to search in nested objects (0 = top level
                        // only)

    // Multiple conditions support (AND logic)
    std::vector<FilterCondition> conditions;

    // Methods
    bool matches(const std::string& value) const;
    bool matches(const db::LogEvent& event) const;
    bool matchesParameter(const db::LogEvent& event) const;
    std::vector<unsigned long> applyToIndices(const std::vector<unsigned long>& inputIndices, const mvc::IModel& model) const;
    void compile();
    bool searchParameterRecursive(
        const std::vector<std::pair<std::string, std::string>>& items,
        const std::string& key, int currentDepth) const;

    // Condition management
    void addCondition(const FilterCondition& condition);
    void removeCondition(size_t index);
    void clearConditions();
    const std::vector<FilterCondition>& getConditions() const;
    bool hasMultipleConditions() const { return conditions.size() > 1; }

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