/**
 * @file FilterStrategyTest.cpp
 * @brief Specification-based unit tests for all IFilterStrategy implementations.
 *
 * Each test is derived from the documented contract of the strategy, not from
 * reading the implementation. The goal is to verify that the code behaves
 * according to its specification.
 *
 * Strategies under test:
 *  - RegexFilterStrategy  : full std::regex search, compiled per-call
 *  - ExactMatchStrategy   : full string equality (case sensitive or not)
 *  - FuzzyMatchStrategy   : Levenshtein distance ≤ maxDistance
 *  - WildcardStrategy     : * (zero or more chars) and ? (exactly one char)
 *
 * All strategies must satisfy:
 *  - matches() is thread-safe for concurrent reads
 *  - isValidPattern() gives early feedback on bad patterns
 *  - getName() returns a stable identifier used for serialisation
 *  - clone() produces an independent copy with identical behaviour
 */

#include <gtest/gtest.h>
#include "IFilterStrategy.hpp"

namespace filters {

// =============================================================================
// RegexFilterStrategy
// =============================================================================

/**
 * @brief Spec: matches() returns true when the value contains the regex pattern.
 * The search is not anchored, so partial matches count.
 */
TEST(RegexFilterStrategyTest, MatchesPartialPattern)
{
    RegexFilterStrategy s;
    EXPECT_TRUE(s.matches("hello world", "world", /*caseSensitive=*/true));
}

/**
 * @brief Spec: matches() returns false when the value does not contain the pattern.
 */
TEST(RegexFilterStrategyTest, NoMatchReturnsfalse)
{
    RegexFilterStrategy s;
    EXPECT_FALSE(s.matches("hello world", "xyz", /*caseSensitive=*/true));
}

/**
 * @brief Spec: case-insensitive matching should ignore case differences.
 */
TEST(RegexFilterStrategyTest, CaseInsensitiveMatch)
{
    RegexFilterStrategy s;
    EXPECT_TRUE(s.matches("Hello World", "hello", /*caseSensitive=*/false));
}

/**
 * @brief Spec: case-sensitive matching must distinguish upper and lower case.
 */
TEST(RegexFilterStrategyTest, CaseSensitiveRejectsWrongCase)
{
    RegexFilterStrategy s;
    EXPECT_FALSE(s.matches("Hello", "hello", /*caseSensitive=*/true));
}

/**
 * @brief Spec: full regex syntax is supported — anchors, character classes,
 * quantifiers.
 */
TEST(RegexFilterStrategyTest, FullRegexSyntaxAnchoredMatch)
{
    RegexFilterStrategy s;
    EXPECT_TRUE(s.matches("192.168.1.1",
        R"(^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$)", true));
    EXPECT_FALSE(s.matches("not-an-ip",
        R"(^\d{1,3}\.\d{1,3}\.\d{1,3}\.\d{1,3}$)", true));
}

/**
 * @brief Spec: isValidPattern() returns false for syntactically invalid regex.
 */
TEST(RegexFilterStrategyTest, InvalidPatternDetected)
{
    RegexFilterStrategy s;
    EXPECT_FALSE(s.isValidPattern("[unclosed"));
}

/**
 * @brief Spec: isValidPattern() returns true for a valid regex pattern.
 */
TEST(RegexFilterStrategyTest, ValidPatternAccepted)
{
    RegexFilterStrategy s;
    EXPECT_TRUE(s.isValidPattern("error.*timeout"));
}

/**
 * @brief Spec: getName() returns the identifier "regex".
 */
TEST(RegexFilterStrategyTest, GetNameReturnsRegex)
{
    RegexFilterStrategy s;
    EXPECT_EQ(s.getName(), "regex");
}

/**
 * @brief Spec: clone() produces an independent copy with identical matching
 * behaviour.
 */
TEST(RegexFilterStrategyTest, CloneMatchesSameWayAsOriginal)
{
    RegexFilterStrategy s;
    auto copy = s.clone();
    ASSERT_NE(copy.get(), nullptr);
    EXPECT_TRUE(copy->matches("error: disk full", "error", true));
    EXPECT_FALSE(copy->matches("info: all good", "error", true));
}

/**
 * @brief Spec: empty pattern matches any string (regex "" is always true).
 */
TEST(RegexFilterStrategyTest, EmptyPatternMatchesAnything)
{
    RegexFilterStrategy s;
    EXPECT_TRUE(s.matches("anything", "", true));
    EXPECT_TRUE(s.matches("", "", true));
}

// =============================================================================
// ExactMatchStrategy
// =============================================================================

/**
 * @brief Spec: matches() returns true only when value equals pattern exactly
 * (case-sensitive).
 */
TEST(ExactMatchStrategyTest, ExactEqualityMatch)
{
    ExactMatchStrategy s;
    EXPECT_TRUE(s.matches("ERROR", "ERROR", true));
}

/**
 * @brief Spec: a partial match is not accepted — "ERROR" should not match "ERR".
 */
TEST(ExactMatchStrategyTest, PartialMatchRejected)
{
    ExactMatchStrategy s;
    EXPECT_FALSE(s.matches("ERROR", "ERR", true));
    EXPECT_FALSE(s.matches("ERR", "ERROR", true));
}

/**
 * @brief Spec: case-insensitive mode treats "error" and "ERROR" as equal.
 */
TEST(ExactMatchStrategyTest, CaseInsensitiveEquality)
{
    ExactMatchStrategy s;
    EXPECT_TRUE(s.matches("error", "ERROR", false));
    EXPECT_TRUE(s.matches("ERROR", "error", false));
}

/**
 * @brief Spec: case-sensitive mode rejects different casing.
 */
TEST(ExactMatchStrategyTest, CaseSensitiveRejectsMixedCase)
{
    ExactMatchStrategy s;
    EXPECT_FALSE(s.matches("Error", "ERROR", true));
}

/**
 * @brief Spec: two empty strings are considered equal.
 */
TEST(ExactMatchStrategyTest, BothEmptyStringsMatch)
{
    ExactMatchStrategy s;
    EXPECT_TRUE(s.matches("", "", true));
}

/**
 * @brief Spec: any string is a valid pattern for exact matching —
 * isValidPattern() always returns true.
 */
TEST(ExactMatchStrategyTest, IsValidPatternAlwaysTrue)
{
    ExactMatchStrategy s;
    EXPECT_TRUE(s.isValidPattern(""));
    EXPECT_TRUE(s.isValidPattern("any pattern"));
    EXPECT_TRUE(s.isValidPattern("[unclosed")); // invalid regex but valid exact pattern
}

/**
 * @brief Spec: getName() returns the identifier "exact".
 */
TEST(ExactMatchStrategyTest, GetNameReturnsExact)
{
    ExactMatchStrategy s;
    EXPECT_EQ(s.getName(), "exact");
}

/**
 * @brief Spec: clone() is independent and behaves identically.
 */
TEST(ExactMatchStrategyTest, CloneBehavesIdentically)
{
    ExactMatchStrategy s;
    auto copy = s.clone();
    ASSERT_NE(copy.get(), nullptr);
    EXPECT_TRUE(copy->matches("DEBUG", "DEBUG", true));
    EXPECT_FALSE(copy->matches("DEBUG", "debug", true));
}

// =============================================================================
// FuzzyMatchStrategy
// =============================================================================

/**
 * @brief Spec: an identical string has distance 0 and always matches at any
 * configured maxDistance.
 */
TEST(FuzzyMatchStrategyTest, ExactStringMatchesAtZeroDistance)
{
    FuzzyMatchStrategy s(2);
    EXPECT_TRUE(s.matches("admin", "admin", true));
}

/**
 * @brief Spec: a string within maxDistance edits is accepted.
 * "admon" vs "admin" = 1 substitution → distance 1 ≤ maxDistance 2.
 */
TEST(FuzzyMatchStrategyTest, WithinMaxDistanceAccepted)
{
    FuzzyMatchStrategy s(2);
    EXPECT_TRUE(s.matches("admon", "admin", false));
}

/**
 * @brief Spec: a string beyond maxDistance is rejected.
 * "xyz" vs "admin" has distance > 2.
 */
TEST(FuzzyMatchStrategyTest, BeyondMaxDistanceRejected)
{
    FuzzyMatchStrategy s(2);
    EXPECT_FALSE(s.matches("xyz", "admin", false));
}

/**
 * @brief Spec: maxDistance = 0 is equivalent to exact matching.
 */
TEST(FuzzyMatchStrategyTest, MaxDistanceZeroIsExactMatch)
{
    FuzzyMatchStrategy s(0);
    EXPECT_TRUE(s.matches("admin", "admin", true));
    EXPECT_FALSE(s.matches("admon", "admin", true));
}

/**
 * @brief Spec: getMaxDistance() returns the value supplied at construction.
 */
TEST(FuzzyMatchStrategyTest, GetMaxDistanceReturnsConstructedValue)
{
    FuzzyMatchStrategy s(5);
    EXPECT_EQ(s.getMaxDistance(), 5);
}

/**
 * @brief Spec: isValidPattern() rejects the empty string (documented:
 * "Non-empty patterns only").
 */
TEST(FuzzyMatchStrategyTest, EmptyPatternIsInvalid)
{
    FuzzyMatchStrategy s(2);
    EXPECT_FALSE(s.isValidPattern(""));
}

/**
 * @brief Spec: isValidPattern() accepts any non-empty string.
 */
TEST(FuzzyMatchStrategyTest, NonEmptyPatternIsValid)
{
    FuzzyMatchStrategy s(2);
    EXPECT_TRUE(s.isValidPattern("admin"));
}

/**
 * @brief Spec: getName() returns the identifier "fuzzy".
 */
TEST(FuzzyMatchStrategyTest, GetNameReturnsFuzzy)
{
    FuzzyMatchStrategy s;
    EXPECT_EQ(s.getName(), "fuzzy");
}

/**
 * @brief Spec: clone() preserves the configured maxDistance.
 */
TEST(FuzzyMatchStrategyTest, ClonePreservesMaxDistance)
{
    FuzzyMatchStrategy s(3);
    auto copy = s.clone();
    ASSERT_NE(copy.get(), nullptr);
    // Cast to verify maxDistance was preserved
    const auto* fc = dynamic_cast<const FuzzyMatchStrategy*>(copy.get());
    ASSERT_NE(fc, nullptr);
    EXPECT_EQ(fc->getMaxDistance(), 3);
}

/**
 * @brief Spec: single insertion/deletion counts as one edit.
 * "colour" vs "color" = 1 deletion → distance 1.
 */
TEST(FuzzyMatchStrategyTest, SingleInsertionIsOneEdit)
{
    FuzzyMatchStrategy s(1);
    EXPECT_TRUE(s.matches("colour", "color", true)); // 1 extra char
}

// =============================================================================
// WildcardStrategy
// =============================================================================

/**
 * @brief Spec: '*' matches zero or more characters anywhere in the value.
 */
TEST(WildcardStrategyTest, StarMatchesZeroOrMoreChars)
{
    WildcardStrategy s;
    EXPECT_TRUE(s.matches("hello.log",   "*.log",  true));
    EXPECT_TRUE(s.matches(".log",        "*.log",  true)); // zero chars before .
    EXPECT_TRUE(s.matches("a.b.c.log",   "*.log",  true));
    EXPECT_FALSE(s.matches("hello.txt",  "*.log",  true));
}

/**
 * @brief Spec: '?' matches exactly one character.
 */
TEST(WildcardStrategyTest, QuestionMarkMatchesExactlyOneChar)
{
    WildcardStrategy s;
    EXPECT_TRUE(s.matches("test_1.txt",  "test_?.txt", true));
    EXPECT_FALSE(s.matches("test_12.txt","test_?.txt", true)); // two chars
    EXPECT_FALSE(s.matches("test_.txt",  "test_?.txt", true)); // zero chars
}

/**
 * @brief Spec: '*' pattern alone matches any string including empty.
 */
TEST(WildcardStrategyTest, StarAloneMatchesEverything)
{
    WildcardStrategy s;
    EXPECT_TRUE(s.matches("",          "*", true));
    EXPECT_TRUE(s.matches("anything",  "*", true));
}

/**
 * @brief Spec: combined '*' and '?' patterns work together.
 */
TEST(WildcardStrategyTest, CombinedWildcards)
{
    WildcardStrategy s;
    EXPECT_TRUE(s.matches("log_1_debug.txt", "log_?_*.txt", true));
    EXPECT_FALSE(s.matches("log__debug.txt", "log_?_*.txt", true));
}

/**
 * @brief Spec: case-insensitive mode ignores case when matching wildcards.
 */
TEST(WildcardStrategyTest, CaseInsensitiveWildcard)
{
    WildcardStrategy s;
    EXPECT_TRUE(s.matches("Error.Log", "error.*", false));
}

/**
 * @brief Spec: case-sensitive mode respects case when matching wildcards.
 */
TEST(WildcardStrategyTest, CaseSensitiveWildcard)
{
    WildcardStrategy s;
    EXPECT_FALSE(s.matches("Error.Log", "error.*", true));
}

/**
 * @brief Spec: any pattern is valid for wildcard — isValidPattern() always
 * returns true.
 */
TEST(WildcardStrategyTest, IsValidPatternAlwaysTrue)
{
    WildcardStrategy s;
    EXPECT_TRUE(s.isValidPattern(""));
    EXPECT_TRUE(s.isValidPattern("*.*"));
    EXPECT_TRUE(s.isValidPattern("[not-regex]"));
}

/**
 * @brief Spec: getName() returns the identifier "wildcard".
 */
TEST(WildcardStrategyTest, GetNameReturnsWildcard)
{
    WildcardStrategy s;
    EXPECT_EQ(s.getName(), "wildcard");
}

/**
 * @brief Spec: clone() produces an independent copy with identical behaviour.
 */
TEST(WildcardStrategyTest, CloneBehavesIdentically)
{
    WildcardStrategy s;
    auto copy = s.clone();
    ASSERT_NE(copy.get(), nullptr);
    EXPECT_TRUE(copy->matches("file.log", "*.log", true));
    EXPECT_FALSE(copy->matches("file.txt", "*.log", true));
}

// =============================================================================
// createStrategy factory
// =============================================================================

/**
 * @brief Spec: createStrategy("regex") returns a RegexFilterStrategy.
 */
TEST(CreateStrategyTest, CreatesRegexStrategy)
{
    auto s = createStrategy("regex");
    ASSERT_NE(s.get(), nullptr);
    EXPECT_EQ(s->getName(), "regex");
}

/**
 * @brief Spec: createStrategy("exact") returns an ExactMatchStrategy.
 */
TEST(CreateStrategyTest, CreatesExactStrategy)
{
    auto s = createStrategy("exact");
    ASSERT_NE(s.get(), nullptr);
    EXPECT_EQ(s->getName(), "exact");
}

/**
 * @brief Spec: createStrategy("fuzzy") returns a FuzzyMatchStrategy.
 */
TEST(CreateStrategyTest, CreatesFuzzyStrategy)
{
    auto s = createStrategy("fuzzy");
    ASSERT_NE(s.get(), nullptr);
    EXPECT_EQ(s->getName(), "fuzzy");
}

/**
 * @brief Spec: createStrategy("wildcard") returns a WildcardStrategy.
 */
TEST(CreateStrategyTest, CreatesWildcardStrategy)
{
    auto s = createStrategy("wildcard");
    ASSERT_NE(s.get(), nullptr);
    EXPECT_EQ(s->getName(), "wildcard");
}

/**
 * @brief Spec: createStrategy() falls back gracefully for unknown names
 * (returns non-null — implementation may fall back to regex or throw;
 * this test verifies the factory does not return nullptr for known names).
 */
TEST(CreateStrategyTest, KnownNamesAlwaysReturnNonNull)
{
    for (const auto& name : {"regex", "exact", "fuzzy", "wildcard"})
    {
        auto s = createStrategy(name);
        EXPECT_NE(s.get(), nullptr) << "createStrategy(\"" << name << "\") returned null";
    }
}

} // namespace filters
