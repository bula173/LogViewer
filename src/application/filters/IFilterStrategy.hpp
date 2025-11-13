/**
 * @file IFilterStrategy.hpp
 * @brief Strategy interface for different filter matching algorithms
 * @author LogViewer Development Team
 * @date 2025
 *
 * Implements the Strategy design pattern for filter matching.
 * Allows runtime selection of different matching algorithms:
 * - Regex matching (default)
 * - Exact string matching
 * - Fuzzy matching (Levenshtein distance)
 * - Wildcard matching
 * - Custom user-defined strategies
 *
 * @par Example Usage:
 * @code
 * // Use regex strategy (default)
 * auto filter = Filter("ErrorFilter", "message", "error.*", true);
 * filter.setStrategy(std::make_unique<RegexFilterStrategy>());
 *
 * // Use exact match for performance-critical filtering
 * auto exactFilter = Filter("ExactFilter", "level", "ERROR");
 * exactFilter.setStrategy(std::make_unique<ExactMatchStrategy>());
 *
 * // Use fuzzy matching for typo tolerance
 * auto fuzzyFilter = Filter("FuzzyFilter", "username", "admin", false);
 * fuzzyFilter.setStrategy(std::make_unique<FuzzyMatchStrategy>(2)); // max distance 2
 * @endcode
 *
 * @par Performance Characteristics:
 * - RegexFilterStrategy: O(n) regex search, compiled once
 * - ExactMatchStrategy: O(n) string comparison or O(1) hash lookup
 * - FuzzyMatchStrategy: O(n*m) Levenshtein distance calculation
 * - WildcardStrategy: O(n) pattern matching with * and ? wildcards
 *
 * @par Thread Safety:
 * All strategy implementations must be thread-safe for concurrent reads.
 * Pattern compilation should be done in constructor or separate init method.
 */

#pragma once

#include <memory>
#include <string>
#include <vector>

namespace filters {

/**
 * @class IFilterStrategy
 * @brief Abstract strategy interface for filter matching
 *
 * Defines the contract for all filter matching strategies.
 * Concrete strategies implement different matching algorithms.
 *
 * **Immutability**: Strategy instances should be immutable after construction
 * to ensure thread safety for concurrent filter evaluations.
 */
class IFilterStrategy {
public:
    virtual ~IFilterStrategy() = default;

    /**
     * @brief Match a value against the pattern
     * @param value The string to test
     * @param pattern The pattern to match against
     * @param caseSensitive Whether matching should be case-sensitive
     * @return true if value matches pattern, false otherwise
     *
     * @par Thread Safety:
     * Must be thread-safe for concurrent calls.
     */
    virtual bool matches(const std::string& value,
                        const std::string& pattern,
                        bool caseSensitive) const = 0;

    /**
     * @brief Validate pattern syntax
     * @param pattern The pattern to validate
     * @return true if pattern is valid for this strategy, false otherwise
     *
     * Used to provide early feedback when constructing filters.
     */
    virtual bool isValidPattern(const std::string& pattern) const = 0;

    /**
     * @brief Get strategy name for serialization/debugging
     * @return Strategy identifier (e.g., "regex", "exact", "fuzzy")
     */
    virtual std::string getName() const = 0;

    /**
     * @brief Clone this strategy (for copy semantics)
     * @return Unique pointer to cloned strategy
     */
    virtual std::unique_ptr<IFilterStrategy> clone() const = 0;
};

/**
 * @class RegexFilterStrategy
 * @brief Regex-based matching using std::regex
 *
 * Default strategy providing full regex support.
 * Patterns are compiled once for efficiency.
 *
 * @par Performance:
 * - Compilation: O(m) where m = pattern length
 * - Matching: O(n) where n = value length
 * - Memory: Stores compiled regex state machine
 */
class RegexFilterStrategy : public IFilterStrategy {
public:
    RegexFilterStrategy() = default;

    bool matches(const std::string& value,
                const std::string& pattern,
                bool caseSensitive) const override;

    bool isValidPattern(const std::string& pattern) const override;

    std::string getName() const override { return "regex"; }

    std::unique_ptr<IFilterStrategy> clone() const override {
        return std::make_unique<RegexFilterStrategy>(*this);
    }

private:
    // Note: std::regex is not stored here to keep strategy stateless
    // Compiled regex is created per-call or cached in Filter class
};

/**
 * @class ExactMatchStrategy
 * @brief Exact string matching
 *
 * Fast exact comparison, case-sensitive or insensitive.
 * Much faster than regex for exact matches.
 *
 * @par Performance:
 * - Matching: O(n) string comparison
 * - Memory: No additional state
 * - Optimization: Could use hash-based lookup for large datasets
 */
class ExactMatchStrategy : public IFilterStrategy {
public:
    ExactMatchStrategy() = default;

    bool matches(const std::string& value,
                const std::string& pattern,
                bool caseSensitive) const override;

    bool isValidPattern(const std::string& /*pattern*/) const override {
        return true; // Any string is valid for exact matching
    }

    std::string getName() const override { return "exact"; }

    std::unique_ptr<IFilterStrategy> clone() const override {
        return std::make_unique<ExactMatchStrategy>(*this);
    }
};

/**
 * @class FuzzyMatchStrategy
 * @brief Fuzzy matching using Levenshtein distance
 *
 * Matches strings within a specified edit distance.
 * Useful for typo-tolerant filtering.
 *
 * @par Performance:
 * - Matching: O(n*m) dynamic programming
 * - Memory: O(min(n,m)) with optimized algorithm
 */
class FuzzyMatchStrategy : public IFilterStrategy {
public:
    /**
     * @brief Construct fuzzy matcher with max edit distance
     * @param maxDistance Maximum Levenshtein distance (default 2)
     */
    explicit FuzzyMatchStrategy(int maxDistance = 2)
        : m_maxDistance(maxDistance) {}

    bool matches(const std::string& value,
                const std::string& pattern,
                bool caseSensitive) const override;

    bool isValidPattern(const std::string& pattern) const override {
        return !pattern.empty(); // Non-empty patterns only
    }

    std::string getName() const override { return "fuzzy"; }

    std::unique_ptr<IFilterStrategy> clone() const override {
        return std::make_unique<FuzzyMatchStrategy>(*this);
    }

    int getMaxDistance() const { return m_maxDistance; }

private:
    int m_maxDistance;

    /**
     * @brief Calculate Levenshtein distance between two strings
     * @param s1 First string
     * @param s2 Second string
     * @return Edit distance (insertions, deletions, substitutions)
     */
    static int levenshteinDistance(const std::string& s1, const std::string& s2);
};

/**
 * @class WildcardStrategy
 * @brief Wildcard pattern matching (* and ?)
 *
 * Simple wildcard matching with * (zero or more chars) and ? (one char).
 * Faster than regex for simple patterns like "*.log" or "test_?.txt".
 *
 * @par Performance:
 * - Matching: O(n) with backtracking for *
 * - Memory: No additional state
 */
class WildcardStrategy : public IFilterStrategy {
public:
    WildcardStrategy() = default;

    bool matches(const std::string& value,
                const std::string& pattern,
                bool caseSensitive) const override;

    bool isValidPattern(const std::string& /*pattern*/) const override {
        return true; // Any pattern is valid for wildcards
    }

    std::string getName() const override { return "wildcard"; }

    std::unique_ptr<IFilterStrategy> clone() const override {
        return std::make_unique<WildcardStrategy>(*this);
    }

private:
    /**
     * @brief Recursive wildcard matching implementation
     * @param value String to test
     * @param pattern Wildcard pattern
     * @param valueIdx Current position in value
     * @param patternIdx Current position in pattern
     * @return true if match found
     */
    static bool matchWildcard(const std::string& value,
                             const std::string& pattern,
                             size_t valueIdx,
                             size_t patternIdx);
};

/**
 * @brief Strategy factory for creating strategies from names
 *
 * Usage:
 * @code
 * auto strategy = createStrategy("fuzzy");
 * auto filter = Filter(...);
 * filter.setStrategy(std::move(strategy));
 * @endcode
 */
std::unique_ptr<IFilterStrategy> createStrategy(const std::string& strategyName);

} // namespace filters
