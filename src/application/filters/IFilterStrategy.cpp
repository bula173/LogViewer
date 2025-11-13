/**
 * @file IFilterStrategy.cpp
 * @brief Implementation of filter matching strategies
 * @author LogViewer Development Team
 * @date 2025
 */

#include "filters/IFilterStrategy.hpp"
#include "util/Logger.hpp"
#include <algorithm>
#include <cctype>
#include <regex>

namespace filters {

namespace {
    /**
     * @brief Convert string to lowercase for case-insensitive comparison
     */
    std::string toLowerCase(const std::string& str) {
        std::string result = str;
        std::transform(result.begin(), result.end(), result.begin(),
            [](unsigned char c) { return std::tolower(c); });
        return result;
    }
} // anonymous namespace

//-----------------------------------------------------------------------------
// RegexFilterStrategy Implementation
//-----------------------------------------------------------------------------

bool RegexFilterStrategy::matches(const std::string& value,
                                  const std::string& pattern,
                                  bool caseSensitive) const
{
    try {
        std::regex::flag_type flags = std::regex::ECMAScript;
        if (!caseSensitive) {
            flags |= std::regex::icase;
        }

        std::regex regex(pattern, flags);
        return std::regex_search(value, regex);

    } catch (const std::regex_error& e) {
        util::Logger::Error("RegexFilterStrategy::matches - Invalid regex '{}': {}",
            pattern, e.what());
        return false;
    }
}

bool RegexFilterStrategy::isValidPattern(const std::string& pattern) const
{
    try {
        std::regex test(pattern, std::regex::ECMAScript);
        return true;
    } catch (const std::regex_error&) {
        return false;
    }
}

//-----------------------------------------------------------------------------
// ExactMatchStrategy Implementation
//-----------------------------------------------------------------------------

bool ExactMatchStrategy::matches(const std::string& value,
                                const std::string& pattern,
                                bool caseSensitive) const
{
    if (caseSensitive) {
        return value == pattern;
    } else {
        return toLowerCase(value) == toLowerCase(pattern);
    }
}

//-----------------------------------------------------------------------------
// FuzzyMatchStrategy Implementation
//-----------------------------------------------------------------------------

int FuzzyMatchStrategy::levenshteinDistance(const std::string& s1, 
                                           const std::string& s2)
{
    const size_t len1 = s1.size();
    const size_t len2 = s2.size();

    // Optimize memory: use only 2 rows instead of full matrix
    std::vector<int> prev(len2 + 1);
    std::vector<int> curr(len2 + 1);

    // Initialize first row
    for (size_t j = 0; j <= len2; ++j) {
        prev[j] = static_cast<int>(j);
    }

    // Fill matrix
    for (size_t i = 1; i <= len1; ++i) {
        curr[0] = static_cast<int>(i);

        for (size_t j = 1; j <= len2; ++j) {
            int cost = (s1[i - 1] == s2[j - 1]) ? 0 : 1;

            curr[j] = std::min({
                prev[j] + 1,      // deletion
                curr[j - 1] + 1,  // insertion
                prev[j - 1] + cost // substitution
            });
        }

        std::swap(prev, curr);
    }

    return prev[len2];
}

bool FuzzyMatchStrategy::matches(const std::string& value,
                                const std::string& pattern,
                                bool caseSensitive) const
{
    std::string v = caseSensitive ? value : toLowerCase(value);
    std::string p = caseSensitive ? pattern : toLowerCase(pattern);

    // Check if pattern appears as substring first (fast path)
    if (v.find(p) != std::string::npos) {
        return true;
    }

    // Calculate edit distance
    int distance = levenshteinDistance(v, p);
    
    util::Logger::Debug("FuzzyMatchStrategy::matches - Distance between '{}' and '{}': {}",
        v, p, distance);

    return distance <= m_maxDistance;
}

//-----------------------------------------------------------------------------
// WildcardStrategy Implementation
//-----------------------------------------------------------------------------

bool WildcardStrategy::matchWildcard(const std::string& value,
                                    const std::string& pattern,
                                    size_t valueIdx,
                                    size_t patternIdx)
{
    const size_t valueLen = value.length();
    const size_t patternLen = pattern.length();

    // Base cases
    if (patternIdx == patternLen) {
        return valueIdx == valueLen; // Pattern consumed, value should be too
    }

    if (valueIdx == valueLen) {
        // Value consumed, pattern should only contain '*'
        while (patternIdx < patternLen && pattern[patternIdx] == '*') {
            ++patternIdx;
        }
        return patternIdx == patternLen;
    }

    // Handle wildcards
    if (pattern[patternIdx] == '*') {
        // Try matching zero characters
        if (matchWildcard(value, pattern, valueIdx, patternIdx + 1)) {
            return true;
        }
        // Try matching one or more characters
        return matchWildcard(value, pattern, valueIdx + 1, patternIdx);
    }

    if (pattern[patternIdx] == '?' || 
        pattern[patternIdx] == value[valueIdx]) {
        // Single character match or '?' wildcard
        return matchWildcard(value, pattern, valueIdx + 1, patternIdx + 1);
    }

    // No match
    return false;
}

bool WildcardStrategy::matches(const std::string& value,
                              const std::string& pattern,
                              bool caseSensitive) const
{
    std::string v = caseSensitive ? value : toLowerCase(value);
    std::string p = caseSensitive ? pattern : toLowerCase(pattern);

    return matchWildcard(v, p, 0, 0);
}

//-----------------------------------------------------------------------------
// Strategy Factory
//-----------------------------------------------------------------------------

std::unique_ptr<IFilterStrategy> createStrategy(const std::string& strategyName)
{
    if (strategyName == "regex") {
        return std::make_unique<RegexFilterStrategy>();
    }
    else if (strategyName == "exact") {
        return std::make_unique<ExactMatchStrategy>();
    }
    else if (strategyName == "fuzzy") {
        return std::make_unique<FuzzyMatchStrategy>(2); // default max distance
    }
    else if (strategyName == "wildcard") {
        return std::make_unique<WildcardStrategy>();
    }
    else {
        util::Logger::Warn("createStrategy - Unknown strategy '{}', defaulting to regex",
            strategyName);
        return std::make_unique<RegexFilterStrategy>();
    }
}

} // namespace filters
