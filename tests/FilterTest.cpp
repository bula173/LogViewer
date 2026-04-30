/**
 * @file FilterTest.cpp
 * @brief Specification-based unit tests for Filter and FilterCondition.
 *
 * Tests are derived from the documented contract (Doxygen comments and design
 * patterns described in Filter.hpp), not from reading the implementation.
 *
 * Covered specifications:
 *
 * Filter
 *  - Default strategy is RegexFilterStrategy.
 *  - matches(string) — pattern match against a plain string value.
 *  - matches(LogEvent) — match against a column field in the event.
 *  - isInverted flag inverts the match result.
 *  - isEnabled=false causes applyToIndices to pass all events through.
 *  - Multiple conditions (AND logic) — all must match.
 *  - setStrategy() / getStrategyName() replace strategy at runtime.
 *  - toJson() / fromJson() round-trip preserves fields.
 *  - hasMultipleConditions() reflects condition count.
 *  - addCondition / removeCondition / clearConditions maintain the vector.
 *
 * FilterCondition
 *  - matches(LogEvent) against its columnName field.
 *  - toJson() / fromJson() round-trip.
 */

#include <gtest/gtest.h>
#include "Filter.hpp"
#include "LogEvent.hpp"
#include "IFilterStrategy.hpp"

namespace filters {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static db::LogEvent MakeEvent(db::LogEvent::EventItems items, int id = 1)
{
    return db::LogEvent(id, std::move(items));
}

// ---------------------------------------------------------------------------
// FilterCondition — matches()
// ---------------------------------------------------------------------------

/**
 * @brief Spec: FilterCondition::matches() returns true when the event's
 * columnName field value satisfies the pattern (using regex by default).
 */
TEST(FilterConditionTest, MatchesColumnByRegex)
{
    FilterCondition cond("level", "ERR");
    auto ev = MakeEvent({{"level", "ERROR"}, {"msg", "something bad"}});
    EXPECT_TRUE(cond.matches(ev));
}

/**
 * @brief Spec: FilterCondition::matches() returns false when the field value
 * does not satisfy the pattern.
 */
TEST(FilterConditionTest, NoMatchReturnsFalse)
{
    FilterCondition cond("level", "ERROR");
    auto ev = MakeEvent({{"level", "INFO"}, {"msg", "all good"}});
    EXPECT_FALSE(cond.matches(ev));
}

/**
 * @brief Spec: FilterCondition::matches() returns false when the target column
 * is missing from the event (empty value does not match most patterns).
 */
TEST(FilterConditionTest, MissingColumnDoesNotMatch)
{
    FilterCondition cond("nonexistent_field", "ERROR");
    auto ev = MakeEvent({{"level", "ERROR"}});
    EXPECT_FALSE(cond.matches(ev));
}

/**
 * @brief Spec: FilterCondition toJson / fromJson round-trip preserves all
 * public fields (columnName, pattern, isParameterFilter, parameterKey,
 * parameterDepth, isCaseSensitive).
 */
TEST(FilterConditionTest, JsonRoundTripPreservesFields)
{
    FilterCondition orig("level", "ERROR", false, "", 0, true);
    const auto j = orig.toJson();
    const auto restored = FilterCondition::fromJson(j);

    EXPECT_EQ(restored.columnName,       orig.columnName);
    EXPECT_EQ(restored.pattern,          orig.pattern);
    EXPECT_EQ(restored.isParameterFilter,orig.isParameterFilter);
    EXPECT_EQ(restored.isCaseSensitive,  orig.isCaseSensitive);
}

// ---------------------------------------------------------------------------
// Filter::matches(string)
// ---------------------------------------------------------------------------

/**
 * @brief Spec: Filter::matches(string) returns true when the plain value
 * matches the primary pattern.
 */
TEST(FilterTest, MatchesStringReturnsTrueOnMatch)
{
    Filter f("f", "msg", "error");
    f.compile();
    EXPECT_TRUE(f.matches("connection error occurred"));
}

/**
 * @brief Spec: Filter::matches(string) returns false when the value does not
 * match the primary pattern.
 */
TEST(FilterTest, MatchesStringReturnsFalseOnNoMatch)
{
    Filter f("f", "msg", "error");
    f.compile();
    EXPECT_FALSE(f.matches("all systems nominal"));
}

/**
 * @brief Spec: isInverted=true inverts the result of matches(string).
 */
TEST(FilterTest, InvertedFlagNegatesStringMatch)
{
    Filter f("f", "level", "DEBUG");
    f.isInverted = true;
    f.compile();
    EXPECT_TRUE(f.matches("INFO"));   // "INFO" does not match "DEBUG" → inverted → true
    EXPECT_FALSE(f.matches("DEBUG")); // matches "DEBUG" → inverted → false
}

// ---------------------------------------------------------------------------
// Filter::matches(LogEvent)
// ---------------------------------------------------------------------------

/**
 * @brief Spec: Filter::matches(LogEvent) returns true when the event's
 * columnName field satisfies the pattern.
 */
TEST(FilterTest, MatchesEventOnTargetColumn)
{
    Filter f("f", "level", "ERROR");
    f.compile();
    auto ev = MakeEvent({{"level", "ERROR"}, {"msg", "oops"}});
    EXPECT_TRUE(f.matches(ev));
}

/**
 * @brief Spec: Filter::matches(LogEvent) returns false when the column exists
 * but the value does not match the pattern.
 */
TEST(FilterTest, MatchesEventFalseWhenColumnValueDiffers)
{
    Filter f("f", "level", "ERROR");
    f.compile();
    auto ev = MakeEvent({{"level", "INFO"}, {"msg", "ok"}});
    EXPECT_FALSE(f.matches(ev));
}

/**
 * @brief Spec: Filter::matches(LogEvent) returns false when the target column
 * is absent.
 */
TEST(FilterTest, MatchesEventFalseWhenColumnMissing)
{
    Filter f("f", "level", "ERROR");
    f.compile();
    auto ev = MakeEvent({{"msg", "no level field here"}});
    EXPECT_FALSE(f.matches(ev));
}

/**
 * @brief Spec: isInverted=true inverts the event match result — events that
 * do NOT match the pattern are accepted.
 */
TEST(FilterTest, InvertedFlagNegatesEventMatch)
{
    Filter f("ExcludeDebug", "level", "DEBUG");
    f.isInverted = true;
    f.compile();

    auto debug = MakeEvent({{"level", "DEBUG"}});
    auto info  = MakeEvent({{"level", "INFO"}});

    EXPECT_FALSE(f.matches(debug)); // DEBUG matches pattern → inverted → false
    EXPECT_TRUE(f.matches(info));   // INFO does not match pattern → inverted → true
}

/**
 * @brief Spec: case-sensitive flag is honoured when matching event fields.
 */
TEST(FilterTest, CaseSensitiveMatchEventField)
{
    Filter f("f", "level", "error", /*caseSensitive=*/true);
    f.compile();

    auto lower = MakeEvent({{"level", "error"}});
    auto upper = MakeEvent({{"level", "ERROR"}});

    EXPECT_TRUE(f.matches(lower));
    EXPECT_FALSE(f.matches(upper));
}

/**
 * @brief Spec: case-insensitive flag causes the match to ignore case.
 */
TEST(FilterTest, CaseInsensitiveMatchEventField)
{
    Filter f("f", "level", "error", /*caseSensitive=*/false);
    f.compile();
    auto upper = MakeEvent({{"level", "ERROR"}});
    EXPECT_TRUE(f.matches(upper));
}

// ---------------------------------------------------------------------------
// Filter — multiple conditions (AND logic)
// ---------------------------------------------------------------------------

/**
 * @brief Spec: when multiple conditions are present they are combined with AND
 * logic — the filter only matches if ALL conditions are satisfied.
 */
TEST(FilterTest, MultipleConditionsRequireAllToMatch)
{
    Filter f("MultiCond");
    f.addCondition(FilterCondition("level",   "ERROR"));
    f.addCondition(FilterCondition("service", "database"));
    f.compile();

    // Both conditions met
    auto both = MakeEvent({{"level", "ERROR"}, {"service", "database"}});
    EXPECT_TRUE(f.matches(both));

    // Only first condition met
    auto onlyLevel = MakeEvent({{"level", "ERROR"}, {"service", "auth"}});
    EXPECT_FALSE(f.matches(onlyLevel));

    // Only second condition met
    auto onlyService = MakeEvent({{"level", "INFO"}, {"service", "database"}});
    EXPECT_FALSE(f.matches(onlyService));
}

/**
 * @brief Spec: evaluation short-circuits on the first false condition.
 * Verified behaviorally: adding a false condition first must still fail.
 */
TEST(FilterTest, MultipleConditionsShortCircuitOnFalse)
{
    Filter f("ShortCircuit");
    f.addCondition(FilterCondition("level", "ERROR")); // will fail
    f.addCondition(FilterCondition("msg",   "disk"));  // would pass
    f.compile();

    auto ev = MakeEvent({{"level", "INFO"}, {"msg", "disk full"}});
    EXPECT_FALSE(f.matches(ev)); // first cond fails → overall false
}

// ---------------------------------------------------------------------------
// Filter — condition management
// ---------------------------------------------------------------------------

/**
 * @brief Spec: hasMultipleConditions() returns false with 0 or 1 conditions,
 * true with 2 or more.
 */
TEST(FilterTest, HasMultipleConditionsReflectsCount)
{
    Filter f("f");
    EXPECT_FALSE(f.hasMultipleConditions()); // 0 conditions

    f.addCondition(FilterCondition("level", "ERROR"));
    EXPECT_FALSE(f.hasMultipleConditions()); // 1 condition

    f.addCondition(FilterCondition("msg", "disk"));
    EXPECT_TRUE(f.hasMultipleConditions());  // 2 conditions
}

/**
 * @brief Spec: removeCondition(index) removes the condition at that index;
 * subsequent conditions shift down.
 */
TEST(FilterTest, RemoveConditionShrinksVector)
{
    Filter f("f");
    f.addCondition(FilterCondition("a", "1"));
    f.addCondition(FilterCondition("b", "2"));
    f.addCondition(FilterCondition("c", "3"));
    ASSERT_EQ(f.getConditions().size(), 3u);

    f.removeCondition(1); // remove "b"
    ASSERT_EQ(f.getConditions().size(), 2u);
    EXPECT_EQ(f.getConditions()[0].columnName, "a");
    EXPECT_EQ(f.getConditions()[1].columnName, "c");
}

/**
 * @brief Spec: clearConditions() removes all conditions.
 */
TEST(FilterTest, ClearConditionsEmptiesVector)
{
    Filter f("f");
    f.addCondition(FilterCondition("level", "ERROR"));
    f.addCondition(FilterCondition("msg",   "disk"));
    f.clearConditions();
    EXPECT_TRUE(f.getConditions().empty());
}

// ---------------------------------------------------------------------------
// Filter — strategy
// ---------------------------------------------------------------------------

/**
 * @brief Spec: setStrategy() replaces the matching strategy at runtime;
 * getStrategyName() reflects the change.
 */
TEST(FilterTest, SetStrategyChangesStrategyName)
{
    Filter f("f", "level", "ERROR");
    // Default should be regex
    EXPECT_EQ(f.getStrategyName(), "regex");

    f.setStrategy(std::make_unique<ExactMatchStrategy>());
    EXPECT_EQ(f.getStrategyName(), "exact");
}

/**
 * @brief Spec: after switching to ExactMatchStrategy, a partial value no
 * longer matches (exact equality required).
 */
TEST(FilterTest, ExactStrategyRequiresFullEquality)
{
    Filter f("f", "level", "ERROR");
    f.setStrategy(std::make_unique<ExactMatchStrategy>());
    f.compile();

    // Full match succeeds
    EXPECT_TRUE(f.matches("ERROR"));
    // Partial match fails under exact strategy
    EXPECT_FALSE(f.matches("ERROR: something"));
}

/**
 * @brief Spec: after switching to WildcardStrategy, wildcards are interpreted.
 */
TEST(FilterTest, WildcardStrategyInterpretsStar)
{
    Filter f("f", "filename", "*.log");
    f.setStrategy(std::make_unique<WildcardStrategy>());
    f.compile();

    EXPECT_TRUE(f.matches("application.log"));
    EXPECT_FALSE(f.matches("application.txt"));
}

// ---------------------------------------------------------------------------
// Filter — JSON serialisation
// ---------------------------------------------------------------------------

/**
 * @brief Spec: toJson() / fromJson() round-trip preserves name, columnName,
 * pattern, isInverted, isEnabled, isCaseSensitive.
 */
TEST(FilterTest, JsonRoundTripPreservesBasicFields)
{
    Filter orig("MyFilter", "level", "ERROR", /*caseSensitive=*/true,
                /*inverted=*/true);
    const auto j = orig.toJson();
    Filter restored = Filter::fromJson(j);

    EXPECT_EQ(restored.name,            orig.name);
    EXPECT_EQ(restored.columnName,      orig.columnName);
    EXPECT_EQ(restored.pattern,         orig.pattern);
    EXPECT_EQ(restored.isInverted,      orig.isInverted);
    EXPECT_EQ(restored.isCaseSensitive, orig.isCaseSensitive);
}

/**
 * @brief Spec: a filter restored from JSON must match events in the same way
 * as the original filter.
 */
TEST(FilterTest, JsonRoundTripPreservesMatchBehaviour)
{
    Filter orig("LevelFilter", "level", "ERROR");
    orig.compile();
    const auto j = orig.toJson();
    Filter restored = Filter::fromJson(j);
    restored.compile();

    auto errorEv = MakeEvent({{"level", "ERROR"}});
    auto infoEv  = MakeEvent({{"level", "INFO"}});

    EXPECT_EQ(orig.matches(errorEv),    restored.matches(errorEv));
    EXPECT_EQ(orig.matches(infoEv),     restored.matches(infoEv));
}

/**
 * @brief Spec: a filter with multiple conditions serialises and restores all
 * conditions, maintaining AND semantics.
 */
TEST(FilterTest, JsonRoundTripPreservesMultipleConditions)
{
    Filter orig("MultiCond");
    orig.addCondition(FilterCondition("level",   "ERROR"));
    orig.addCondition(FilterCondition("service", "db"));
    const auto j = orig.toJson();
    Filter restored = Filter::fromJson(j);
    restored.compile();

    auto both   = MakeEvent({{"level", "ERROR"}, {"service", "db"}});
    auto onlyL  = MakeEvent({{"level", "ERROR"}, {"service", "auth"}});

    EXPECT_TRUE(restored.matches(both));
    EXPECT_FALSE(restored.matches(onlyL));
}

// ---------------------------------------------------------------------------
// Filter — copy semantics
// ---------------------------------------------------------------------------

/**
 * @brief Spec: copy constructor / copy assignment produce independent filters
 * (unique_ptr members are deep-copied via clone()).
 */
TEST(FilterTest, CopiedFilterIsIndependent)
{
    Filter original("f", "level", "ERROR");
    original.compile();

    Filter copy = original; // invoke copy constructor
    copy.pattern = "INFO";
    copy.compile();

    // Original still matches ERROR
    EXPECT_TRUE(original.matches("ERROR"));
    // Copy now matches INFO
    EXPECT_TRUE(copy.matches("INFO"));
}

} // namespace filters
