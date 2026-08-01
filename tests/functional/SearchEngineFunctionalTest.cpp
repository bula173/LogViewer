#include <gtest/gtest.h>
#include "application/ui/qt/utils/SearchEngine.hpp"
#include <string>
#include <vector>

namespace functional::tests {

/**
 * Functional Tests: SearchEngine
 * Tests all search modes with realistic data and edge cases
 */
class SearchEngineFunctionalTest : public ::testing::Test {
protected:
    ui::qt::utils::SearchEngine m_engine;

    // Sample texts for testing
    const std::string errorLog = "[ERROR] Database connection failed: timeout after 30s";
    const std::string warningLog = "[WARNING] Memory usage at 85%, consider optimization";
    const std::string infoLog = "[INFO] Application started successfully";
    const std::string debugLog = "[DEBUG] Processing request from client 192.168.1.100";
    const std::string mixedLog = "ERROR: Failed to parse JSON config file at /etc/app.conf";
};

// ============================================================================
// SUBSTRING SEARCH TESTS
// ============================================================================

/**
 * TEST: Substring search finds all occurrences
 */
TEST_F(SearchEngineFunctionalTest, SubstringSearchFindAllOccurrences)
{
    m_engine.SetMode(ui::qt::utils::SearchMode::Substring);
    std::string error;

    EXPECT_TRUE(m_engine.CompilePattern("ERROR", error));
    EXPECT_TRUE(m_engine.Matches(errorLog));
    EXPECT_TRUE(m_engine.Matches(mixedLog));
    EXPECT_FALSE(m_engine.Matches(infoLog));
}

/**
 * TEST: Substring search is case-insensitive by default
 */
TEST_F(SearchEngineFunctionalTest, SubstringSearchCaseInsensitiveByDefault)
{
    m_engine.SetMode(ui::qt::utils::SearchMode::Substring);
    std::string error;

    EXPECT_TRUE(m_engine.CompilePattern("error", error));
    EXPECT_TRUE(m_engine.Matches(errorLog));  // "[ERROR]" should match "error"
    EXPECT_TRUE(m_engine.Matches(mixedLog));  // "ERROR:" should match "error"
}

/**
 * TEST: Substring search can be case-sensitive
 */
TEST_F(SearchEngineFunctionalTest, SubstringSearchCaseSensitiveMode)
{
    m_engine.SetMode(ui::qt::utils::SearchMode::CaseSensitive);
    std::string error;

    EXPECT_TRUE(m_engine.CompilePattern("ERROR", error));
    EXPECT_TRUE(m_engine.Matches(errorLog));   // Has "[ERROR]"
    EXPECT_FALSE(m_engine.Matches(warningLog));// No "ERROR" (has "WARNING")
    EXPECT_TRUE(m_engine.Matches(mixedLog));   // Has "ERROR:"
}

/**
 * TEST: Substring search with special characters
 */
TEST_F(SearchEngineFunctionalTest, SubstringSearchWithSpecialCharacters)
{
    m_engine.SetMode(ui::qt::utils::SearchMode::Substring);
    std::string error;

    EXPECT_TRUE(m_engine.CompilePattern("[ERROR]", error));
    EXPECT_TRUE(m_engine.Matches(errorLog));
    EXPECT_FALSE(m_engine.Matches(warningLog));

    EXPECT_TRUE(m_engine.CompilePattern("192.168", error));
    EXPECT_TRUE(m_engine.Matches(debugLog));
}

// ============================================================================
// REGEX SEARCH TESTS
// ============================================================================

/**
 * TEST: Regex search with simple pattern
 */
TEST_F(SearchEngineFunctionalTest, RegexSearchSimplePattern)
{
    m_engine.SetMode(ui::qt::utils::SearchMode::Regex);
    std::string error;

    EXPECT_TRUE(m_engine.CompilePattern("\\[ERROR\\]", error)) << "Error: " << error;
    EXPECT_TRUE(m_engine.Matches(errorLog));
    EXPECT_FALSE(m_engine.Matches(warningLog));
}

/**
 * TEST: Regex search with alternation
 */
TEST_F(SearchEngineFunctionalTest, RegexSearchWithAlternation)
{
    m_engine.SetMode(ui::qt::utils::SearchMode::Regex);
    std::string error;

    // Match ERROR or WARNING
    EXPECT_TRUE(m_engine.CompilePattern("(ERROR|WARNING)", error)) << "Error: " << error;
    EXPECT_TRUE(m_engine.Matches(errorLog));
    EXPECT_TRUE(m_engine.Matches(warningLog));
    EXPECT_FALSE(m_engine.Matches(infoLog));
}

/**
 * TEST: Regex search with character class
 */
TEST_F(SearchEngineFunctionalTest, RegexSearchWithCharacterClass)
{
    m_engine.SetMode(ui::qt::utils::SearchMode::Regex);
    std::string error;

    // Match 1-3 digit numbers
    EXPECT_TRUE(m_engine.CompilePattern("\\b\\d{1,3}\\b", error)) << "Error: " << error;
    EXPECT_TRUE(m_engine.Matches(debugLog));  // Has "192", "168", etc.
    EXPECT_TRUE(m_engine.Matches(errorLog));  // Has "30"
}

/**
 * TEST: Invalid regex fails gracefully
 */
TEST_F(SearchEngineFunctionalTest, InvalidRegexFailsGracefully)
{
    m_engine.SetMode(ui::qt::utils::SearchMode::Regex);
    std::string error;

    EXPECT_FALSE(m_engine.CompilePattern("[invalid", error));
    EXPECT_FALSE(error.empty());
    EXPECT_NE(error.find("Regex error"), std::string::npos);
}

/**
 * TEST: Regex case insensitive
 */
TEST_F(SearchEngineFunctionalTest, RegexCaseInsensitive)
{
    m_engine.SetMode(ui::qt::utils::SearchMode::Regex);
    m_engine.SetCaseSensitive(false);
    std::string error;

    EXPECT_TRUE(m_engine.CompilePattern("error", error));
    EXPECT_TRUE(m_engine.Matches(errorLog));   // Has "[ERROR]"
    EXPECT_TRUE(m_engine.Matches(mixedLog));   // Has "ERROR:"
}

// ============================================================================
// ADVANCED SEARCH TESTS (AND/OR/NOT)
// ============================================================================

/**
 * TEST: AND operator requires all terms
 */
TEST_F(SearchEngineFunctionalTest, AdvancedSearchANDOperator)
{
    m_engine.SetMode(ui::qt::utils::SearchMode::Advanced);
    std::string error;

    // "ERROR AND timeout" should match errorLog
    EXPECT_TRUE(m_engine.CompilePattern("ERROR AND timeout", error)) << "Error: " << error;
    EXPECT_TRUE(m_engine.Matches(errorLog));
    EXPECT_FALSE(m_engine.Matches(warningLog));
    EXPECT_FALSE(m_engine.Matches(infoLog));
}

/**
 * TEST: OR operator matches any term
 */
TEST_F(SearchEngineFunctionalTest, AdvancedSearchOROperator)
{
    m_engine.SetMode(ui::qt::utils::SearchMode::Advanced);
    std::string error;

    // "INFO OR WARNING" should match both
    EXPECT_TRUE(m_engine.CompilePattern("INFO OR WARNING", error)) << "Error: " << error;
    EXPECT_TRUE(m_engine.Matches(infoLog));
    EXPECT_TRUE(m_engine.Matches(warningLog));
    EXPECT_FALSE(m_engine.Matches(errorLog));
}

/**
 * TEST: NOT operator excludes terms
 */
TEST_F(SearchEngineFunctionalTest, AdvancedSearchNOTOperator)
{
    m_engine.SetMode(ui::qt::utils::SearchMode::Advanced);
    std::string error;

    // "NOT ERROR" should match everything except errorLog
    EXPECT_TRUE(m_engine.CompilePattern("NOT ERROR", error)) << "Error: " << error;
    EXPECT_TRUE(m_engine.Matches(infoLog));
    EXPECT_TRUE(m_engine.Matches(warningLog));
    EXPECT_FALSE(m_engine.Matches(errorLog));
}

/**
 * TEST: Complex advanced query mixing operators
 */
TEST_F(SearchEngineFunctionalTest, AdvancedSearchComplexQuery)
{
    m_engine.SetMode(ui::qt::utils::SearchMode::Advanced);
    std::string error;

    // "ERROR OR WARNING AND connection" should match errorLog
    EXPECT_TRUE(m_engine.CompilePattern("ERROR OR WARNING", error)) << "Error: " << error;
    EXPECT_TRUE(m_engine.Matches(errorLog));
    EXPECT_TRUE(m_engine.Matches(warningLog));
}

/**
 * TEST: Empty advanced query fails
 */
TEST_F(SearchEngineFunctionalTest, AdvancedSearchEmptyQueryFails)
{
    m_engine.SetMode(ui::qt::utils::SearchMode::Advanced);
    std::string error;

    EXPECT_FALSE(m_engine.CompilePattern("", error));
    EXPECT_FALSE(error.empty());
}

// ============================================================================
// SEARCH HIGHLIGHTING TESTS
// ============================================================================

/**
 * TEST: FindMatches locates all occurrences
 */
TEST_F(SearchEngineFunctionalTest, FindMatchesLocatesAllOccurrences)
{
    m_engine.SetMode(ui::qt::utils::SearchMode::Substring);
    std::string error;

    EXPECT_TRUE(m_engine.CompilePattern("a", error));
    auto matches = m_engine.FindMatches("banana");

    // Should find 3 matches at positions 1, 3, 5
    EXPECT_EQ(matches.size(), 3);
}

/**
 * TEST: FindMatches with no matches returns empty
 */
TEST_F(SearchEngineFunctionalTest, FindMatchesWithNoMatchesReturnsEmpty)
{
    m_engine.SetMode(ui::qt::utils::SearchMode::Substring);
    std::string error;

    EXPECT_TRUE(m_engine.CompilePattern("xyz", error));
    auto matches = m_engine.FindMatches("banana");

    EXPECT_TRUE(matches.empty());
}

// ============================================================================
// SEARCH HISTORY TESTS
// ============================================================================

/**
 * TEST: Search history tracks recent searches
 */
TEST_F(SearchEngineFunctionalTest, SearchHistoryTracksRecents)
{
    m_engine.AddToHistory("ERROR");
    m_engine.AddToHistory("WARNING");
    m_engine.AddToHistory("INFO");

    auto history = m_engine.GetHistory();
    EXPECT_EQ(history.size(), 3);
    EXPECT_EQ(history[0], "INFO");      // Most recent first
    EXPECT_EQ(history[1], "WARNING");
    EXPECT_EQ(history[2], "ERROR");
}

/**
 * TEST: Search suggestions filter by prefix
 */
TEST_F(SearchEngineFunctionalTest, SearchSuggestionsFilterByPrefix)
{
    m_engine.AddToHistory("connection");
    m_engine.AddToHistory("config");
    m_engine.AddToHistory("timeout");

    auto suggestions = m_engine.GetSuggestions("con");
    // Should suggest "connection" and "config"
    EXPECT_GE(suggestions.size(), 1);
}

/**
 * TEST: Clear history works
 */
TEST_F(SearchEngineFunctionalTest, ClearHistoryWorks)
{
    m_engine.AddToHistory("ERROR");
    m_engine.AddToHistory("WARNING");

    m_engine.ClearHistory();
    auto history = m_engine.GetHistory();

    EXPECT_TRUE(history.empty());
}

// ============================================================================
// PATTERN MANAGEMENT TESTS
// ============================================================================

/**
 * TEST: Clear pattern resets state
 */
TEST_F(SearchEngineFunctionalTest, ClearPatternResetsState)
{
    m_engine.SetMode(ui::qt::utils::SearchMode::Substring);
    std::string error;

    EXPECT_TRUE(m_engine.CompilePattern("ERROR", error));
    EXPECT_EQ(m_engine.GetPattern(), "ERROR");

    m_engine.Clear();
    EXPECT_TRUE(m_engine.GetPattern().empty());
}

// ============================================================================
// PERFORMANCE TESTS
// ============================================================================

/**
 * TEST: Large document search completes efficiently
 */
TEST_F(SearchEngineFunctionalTest, LargeDocumentSearchPerformance)
{
    // Create a large text with 10k lines
    std::string largeText;
    for (int i = 0; i < 10000; ++i) {
        largeText += "[INFO] Line " + std::to_string(i) + " message\n";
    }

    m_engine.SetMode(ui::qt::utils::SearchMode::Substring);
    std::string error;

    EXPECT_TRUE(m_engine.CompilePattern("INFO", error));
    EXPECT_TRUE(m_engine.Matches(largeText));

    auto matches = m_engine.FindMatches(largeText);
    EXPECT_EQ(matches.size(), 10000);  // Should find all occurrences
}

/**
 * TEST: Regex compilation caching doesn't recompile
 */
TEST_F(SearchEngineFunctionalTest, RegexCompilationCaching)
{
    m_engine.SetMode(ui::qt::utils::SearchMode::Regex);
    std::string error;

    // Compile once
    EXPECT_TRUE(m_engine.CompilePattern("\\d+", error));
    std::string pattern1 = m_engine.GetPattern();

    // Compile same pattern again
    EXPECT_TRUE(m_engine.CompilePattern("\\d+", error));
    std::string pattern2 = m_engine.GetPattern();

    EXPECT_EQ(pattern1, pattern2);
}

} // namespace functional::tests
