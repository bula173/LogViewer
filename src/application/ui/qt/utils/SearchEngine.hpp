#pragma once

#include <string>
#include <vector>
#include <memory>
#include <regex>
#include <optional>

namespace ui::qt::utils
{

struct SearchMatch
{
    int startPos;
    int endPos;
};

enum class SearchMode
{
    Substring,    // Simple substring search
    CaseSensitive,// Case-sensitive substring
    Regex,        // Regex pattern
    Advanced      // AND/OR/NOT logic
};

/**
 * @brief Advanced search engine supporting multiple search modes.
 *
 * Supports:
 * - Simple substring search
 * - Case-sensitive search
 * - Regex patterns
 * - Advanced syntax: "term1 AND term2", "term1 OR term2", "NOT term1"
 */
class SearchEngine
{
  public:
    SearchEngine();

    /// Set search mode
    void SetMode(SearchMode mode);
    SearchMode GetMode() const { return m_mode; }

    /// Set case sensitivity for substring/advanced modes
    void SetCaseSensitive(bool sensitive) { m_caseSensitive = sensitive; }
    bool IsCaseSensitive() const { return m_caseSensitive; }

    /// Compile a search pattern (validates regex, parses advanced syntax)
    bool CompilePattern(const std::string& pattern, std::string& outError);

    /// Check if a string matches the current search pattern
    bool Matches(const std::string& text) const;

    /// Find all matches in a string (for highlighting)
    std::vector<SearchMatch> FindMatches(const std::string& text) const;

    /// Get compiled pattern (for display/logging)
    const std::string& GetPattern() const { return m_pattern; }

    /// Clear current pattern
    void Clear();

    /// Add to search history
    void AddToHistory(const std::string& term);

    /// Get search history (most recent first)
    const std::vector<std::string>& GetHistory() const { return m_history; }

    /// Clear history
    void ClearHistory() { m_history.clear(); }

    /// Get suggestions based on current input (for autocomplete)
    std::vector<std::string> GetSuggestions(const std::string& prefix) const;

  private:
    struct AdvancedQuery
    {
        std::vector<std::string> andTerms;
        std::vector<std::string> orTerms;
        std::vector<std::string> notTerms;
    };

    bool compileSimple(const std::string& pattern);
    bool compileRegex(const std::string& pattern, std::string& outError);
    bool compileAdvanced(const std::string& pattern, std::string& outError);
    bool matchesSimple(const std::string& text) const;
    bool matchesRegex(const std::string& text) const;
    bool matchesAdvanced(const std::string& text) const;
    std::vector<SearchMatch> findSimpleMatches(const std::string& text) const;
    std::vector<SearchMatch> findRegexMatches(const std::string& text) const;

    SearchMode m_mode {SearchMode::Substring};
    bool m_caseSensitive {false};
    std::string m_pattern;
    std::optional<std::regex> m_compiledRegex;
    std::optional<AdvancedQuery> m_advancedQuery;
    std::vector<std::string> m_history;
    static constexpr size_t kMaxHistorySize = 50;
};

}  // namespace ui::qt::utils
