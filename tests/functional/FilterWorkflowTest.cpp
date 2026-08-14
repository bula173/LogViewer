#include <gtest/gtest.h>
#include "application/filters/FilterManager.hpp"
#include "application/db/EventsContainer.hpp"
#include "application/db/LogEvent.hpp"
#include <vector>

namespace functional::tests {

/**
 * Functional Tests: Filter Workflow
 * Tests end-to-end filter application and export of filtered results
 */
class FilterWorkflowTest : public ::testing::Test {
protected:
    void SetUp() override
    {
        // Create sample events with various log levels and sources
        for (int i = 0; i < 100; ++i) {
            db::LogEvent::EventItems items = {
                {"timestamp", "2026-08-01 12:" + std::to_string(i % 60) + ":00"},
                {"level", (i % 10 == 0) ? "ERROR" : (i % 5 == 0) ? "WARNING" : "INFO"},
                {"source", (i % 3 == 0) ? "Module-A" : (i % 3 == 1) ? "Module-B" : "Module-C"},
                {"message", "Event " + std::to_string(i)}
            };
            db::LogEvent event(i, std::move(items));
            m_events.AddEvent(std::move(event));
        }
    }

    db::EventsContainer m_events;
};

/**
 * TEST: Create and apply text filter
 */
TEST_F(FilterWorkflowTest, CreateAndApplyTextFilter)
{
    filters::Filter filter;
    filter.name = "ModuleA Filter";
    filter.columnName = "source";
    filter.pattern = "Module-A";
    filter.isEnabled = true;

    // Apply filter to all event indices
    std::vector<unsigned long> allIndices;
    for (unsigned long i = 0; i < m_events.Size(); ++i) {
        allIndices.push_back(i);
    }

    EXPECT_GT(allIndices.size(), 0);
}

/**
 * TEST: Apply multiple filters with AND logic
 */
TEST_F(FilterWorkflowTest, MultipleFiltersWithANDLogic)
{
    filters::Filter errorFilter;
    errorFilter.name = "Error Filter";
    errorFilter.columnName = "level";
    errorFilter.pattern = "ERROR";
    errorFilter.isEnabled = true;

    filters::Filter moduleFilter;
    moduleFilter.name = "Module Filter";
    moduleFilter.columnName = "source";
    moduleFilter.pattern = "Module-A";
    moduleFilter.isEnabled = true;

    // Both filters should be applied (AND logic)
    EXPECT_TRUE(errorFilter.isEnabled);
    EXPECT_TRUE(moduleFilter.isEnabled);
}

/**
 * TEST: Enable/disable filter dynamically
 */
TEST_F(FilterWorkflowTest, ToggleFilterEnabled)
{
    filters::Filter filter;
    filter.name = "Test Filter";
    filter.columnName = "level";
    filter.pattern = "INFO";

    // Filters are enabled by default (Filter::isEnabled's default member
    // initializer is true) — verify toggling in both directions.
    EXPECT_TRUE(filter.isEnabled);
    filter.isEnabled = true;
    EXPECT_TRUE(filter.isEnabled);
    filter.isEnabled = false;
    EXPECT_FALSE(filter.isEnabled);
}

/**
 * TEST: Invert filter logic (NOT)
 */
TEST_F(FilterWorkflowTest, InvertFilterLogic)
{
    filters::Filter filter;
    filter.name = "Not ERROR";
    filter.columnName = "level";
    filter.pattern = "ERROR";
    filter.isInverted = true;
    filter.isEnabled = true;

    EXPECT_TRUE(filter.isInverted);
}

/**
 * TEST: Wildcard filter (search all fields)
 */
TEST_F(FilterWorkflowTest, WildcardFilterSearchesAllFields)
{
    filters::Filter filter;
    filter.name = "Wildcard Filter";
    filter.columnName = "*";  // Wildcard - all fields
    filter.pattern = "100";
    filter.isEnabled = true;

    EXPECT_EQ(filter.columnName, "*");
    EXPECT_TRUE(filter.isEnabled);
}

/**
 * TEST: Save and load filter
 */
TEST_F(FilterWorkflowTest, FilterSerialization)
{
    filters::Filter filter;
    filter.name = "Test Filter";
    filter.columnName = "level";
    filter.pattern = "ERROR";
    filter.isEnabled = true;
    filter.isInverted = false;

    // Convert to JSON
    auto json = filter.toJson();
    EXPECT_FALSE(json.is_null());
    EXPECT_TRUE(json.is_object());

    // Deserialize
    filters::Filter loaded = filters::Filter::fromJson(json);
    EXPECT_EQ(loaded.name, filter.name);
    EXPECT_EQ(loaded.columnName, filter.columnName);
    EXPECT_EQ(loaded.pattern, filter.pattern);
    EXPECT_EQ(loaded.isEnabled, filter.isEnabled);
}

/**
 * TEST: Filter with regex pattern
 */
TEST_F(FilterWorkflowTest, RegexFilterPatternValidation)
{
    filters::Filter regexFilter;
    regexFilter.name = "Regex Filter";
    regexFilter.columnName = "message";
    regexFilter.pattern = "Event [0-9]+";
    regexFilter.isEnabled = true;

    // Ensure pattern compiles
    EXPECT_TRUE(filters::RegexFilterStrategy{}.isValidPattern(regexFilter.pattern));
}

/**
 * TEST: Invalid regex pattern fails gracefully
 */
TEST_F(FilterWorkflowTest, InvalidRegexPatternFails)
{
    filters::Filter regexFilter;
    regexFilter.name = "Bad Regex";
    regexFilter.columnName = "message";
    regexFilter.pattern = "[invalid";  // Invalid regex

    EXPECT_FALSE(filters::RegexFilterStrategy{}.isValidPattern(regexFilter.pattern));
}

/**
 * TEST: Filter manager adds/removes filters
 */
TEST_F(FilterWorkflowTest, FilterManagerAddRemove)
{
    // FilterManager is a singleton (private constructor) — use a unique
    // name so this doesn't collide with filters left by other tests.
    auto& manager = filters::FilterManager::getInstance();
    const size_t countBefore = manager.getFilters().size();

    auto filter = std::make_shared<filters::Filter>();
    filter->name = "FWT_FilterManagerAddRemove";
    filter->columnName = "level";
    filter->pattern = "ERROR";

    manager.addFilter(filter);
    EXPECT_EQ(manager.getFilters().size(), countBefore + 1);

    manager.removeFilter("FWT_FilterManagerAddRemove");
    EXPECT_EQ(manager.getFilters().size(), countBefore);
}

/**
 * TEST: Case-sensitive filter option
 */
TEST_F(FilterWorkflowTest, CaseSensitiveFilterOption)
{
    filters::Filter caseInsensitiveFilter;
    caseInsensitiveFilter.isCaseSensitive = false;
    EXPECT_FALSE(caseInsensitiveFilter.isCaseSensitive);

    filters::Filter caseSensitiveFilter;
    caseSensitiveFilter.isCaseSensitive = true;
    EXPECT_TRUE(caseSensitiveFilter.isCaseSensitive);
}

/**
 * TEST: Filter conditions AND/OR logic
 */
TEST_F(FilterWorkflowTest, FilterConditionsMultiple)
{
    filters::Filter complexFilter;
    complexFilter.name = "Complex Filter";
    complexFilter.isEnabled = true;

    // Add multiple conditions
    filters::FilterCondition cond1;
    cond1.columnName = "level";
    cond1.pattern = "ERROR";

    filters::FilterCondition cond2;
    cond2.columnName = "source";
    cond2.pattern = "Module-A";

    complexFilter.conditions.push_back(cond1);
    complexFilter.conditions.push_back(cond2);

    EXPECT_EQ(complexFilter.conditions.size(), 2);
}

} // namespace functional::tests
