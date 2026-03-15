#pragma once

#include <memory>
#include <regex>
#include <string>
#include <vector>

#include "LogEvent.hpp"
#include "IFilterStrategy.hpp"
#include "IModel.hpp"

#include <nlohmann/json.hpp>

namespace filters
{

/**
 * @struct FilterCondition
 * @brief Represents a single matching condition within a filter structure.
 *
 * A filter can have multiple conditions that are combined with AND logic,
 * allowing complex filtering expressions. Each condition specifies:
 * - A target column or parameter to search
 * - A pattern to match against
 * - Strategy for matching (regex, substring, etc.)
 * - Optional case sensitivity
 *
 * @par Usage Scenarios
 * - **Column Matching**: Match against log event fields (level, timestamp, etc.)
 * - **Parameter Matching**: Search nested JSON structures at configurable depth
 * - **Complex Patterns**: Use regex patterns for sophisticated matching
 * - **Case Handling**: Optional case-sensitive or case-insensitive matching
 *
 * @par Thread Safety
 * FilterCondition is immutable with respect to matching. Once created, the
 * condition can be safely used by multiple threads without synchronization.
 *
 * @par Example
 * @code
 * // Simple column matching
 * FilterCondition level_error(
 *     "level",      // columnName
 *     "ERROR",      // pattern
 *     false,        // parameterFilter
 *     "",           // parameterKey
 *     0,            // depth
 *     true          // caseSensitive
 * );
 *
 * // Nested parameter search
 * FilterCondition nested_search(
 *     "",                    // columnName (not used for parameters)
 *     "connection_timeout",  // pattern to find in nested data
 *     true,                  // isParameterFilter = true
 *     "error_details",       // parameterKey
 *     2                      // depth = 2 levels deep
 * );
 * @endcode
 *
 * @see Filter for combining multiple conditions
 * @see IFilterStrategy for matching algorithm details
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
          parameterKey(paramKey), parameterDepth(depth), isCaseSensitive(caseSensitive),
          strategy(std::make_unique<RegexFilterStrategy>())
    {
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
 * @brief Composable log event filter with pluggable matching strategies and multi-condition support.
 *
 * The Filter class implements a flexible filtering system for log events, supporting:
 * - **Single and Multiple Conditions**: Conditions can be combined with AND logic
 * - **Strategy Pattern**: Pluggable matching algorithms (regex, substring, etc.)
 * - **Complex Queries**: Column matching, nested parameter search, regex patterns
 * - **Inversion**: Supports NOT logic via inverted flag
 * - **Persistence**: Filters can be serialized to JSON and restored
 *
 * @par Design Patterns
 * - **Strategy Pattern**: Uses IFilterStrategy for pluggable matching algorithms
 * - **Builder Pattern**: Legacy fields (name, columnName, pattern) for simple cases
 * - **Composite Pattern**: Multiple conditions combined with AND logic
 *
 * @par Thread Safety
 * Filter instances should not be modified during concurrent matching.
 * Strategy instances are immutable and thread-safe for concurrent reads.
 * For thread-safe filter management, use FilterManager.
 *
 * @par Performance Notes
 * - Matching is O(k * n) where k = number of conditions, n = event fields
 * - Evaluation short-circuits on first false condition
 * - Compiled expressions are cached for efficiency
 *
 * @par Usage Examples
 *
 * **Simple single-condition filter:**
 * @code
 * filters::Filter error_filter("Show Errors", "level", "ERROR");
 * error_filter.compile();
 *
 * if (error_filter.matches(event)) {
 *     // Event passes filter - display it
 * }
 * @endcode
 *
 * **Complex multi-condition filter:**
 * @code
 * filters::Filter complex_filter("Database Errors");
 * complex_filter.addCondition(
 *     FilterCondition("database", "error", false, "", 0, true)
 * );
 * complex_filter.addCondition(
 *     FilterCondition("status", "failed", false, "", 0, false)
 * );
 * complex_filter.compile();
 * @endcode
 *
 * **Inverted filter:**
 * @code
 * filters::Filter not_debug("Exclude Debug");
 * not_debug.columnName = "level";
 * not_debug.pattern = "DEBUG";
 * not_debug.isInverted = true;  // Show everything EXCEPT DEBUG
 * not_debug.compile();
 * @endcode
 *
 * **Regex pattern matching:**
 * @code
 * filters::Filter regex_filter(
 *     "IPv4 Addresses",
 *     "ip_address",
 *     R"(^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$)"  // Regex pattern
 * );
 * regex_filter.compile();
 * @endcode
 *
 * **Serialization:**
 * @code
 * // Save to JSON
 * nlohmann::json j = filter.toJson();
 *
 * // Load from JSON
 * filters::Filter restored =
 *     filters::Filter::fromJson(j);  // Static method
 * @endcode
 *
 * @see FilterManager for managing multiple filters
 * @see FilterCondition for individual condition details
 * @see IFilterStrategy for custom matching strategies
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