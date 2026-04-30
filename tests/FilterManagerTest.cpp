/**
 * @file FilterManagerTest.cpp
 * @brief Specification-based unit tests for FilterManager.
 *
 * FilterManager is a singleton that:
 *  - owns the list of user-defined Filter objects
 *  - rejects duplicate names on addFilter()
 *  - allows replacement via updateFilter()
 *  - persists filters to/from JSON files (saveFiltersToPath / loadFiltersFromPath)
 *  - enables/disables individual or all filters
 *  - applies only enabled filters via applyFilters()
 *
 * Design rules verified here (derived from the documented API, not from
 * looking at the implementation):
 *
 *   1. getInstance() always returns the same object.
 *   2. addFilter() appends a filter; getFilters() reflects the addition.
 *   3. addFilter() silently rejects a second filter with the same name.
 *   4. removeFilter() removes the named filter; others remain.
 *   5. getFilterByName() returns the filter or nullptr.
 *   6. updateFilter() replaces an existing filter by name; if the name is
 *      new the filter is appended.
 *   7. enableFilter() controls whether applyFilters() uses the filter.
 *   8. enableAllFilters(false) disables every filter; applyFilters() then
 *      returns all indices (no filter active).
 *   9. saveFiltersToPath / loadFiltersFromPath round-trip preserves filters.
 *  10. createFilter() returns a shared_ptr with correct fields.
 *
 * NOTE: FilterManager is a singleton, so each test fixture cleans up by
 * removing any filters it added. Tests that add named filters use unique
 * names to avoid cross-test interference.
 */

#include <gtest/gtest.h>
#include <gmock/gmock.h>
#include "FilterManager.hpp"
#include "EventsContainer.hpp"
#include "LogEvent.hpp"
#include <filesystem>
#include <string>

namespace filters {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

/// RAII guard that removes named filters from the singleton after each test.
struct FilterGuard
{
    std::vector<std::string> names;
    ~FilterGuard()
    {
        for (const auto& n : names)
            FilterManager::getInstance().removeFilter(n);
    }
};

static void FillContainer(db::EventsContainer& c,
    std::initializer_list<db::LogEvent::EventItems> rows)
{
    int id = 1;
    for (auto items : rows)
        c.AddEvent(db::LogEvent(id++, std::move(items)));
}

// ---------------------------------------------------------------------------
// Singleton identity
// ---------------------------------------------------------------------------

/**
 * @brief Spec: getInstance() always returns a reference to the same object.
 */
TEST(FilterManagerTest, SingletonIdentity)
{
    FilterManager& a = FilterManager::getInstance();
    FilterManager& b = FilterManager::getInstance();
    EXPECT_EQ(&a, &b);
}

// ---------------------------------------------------------------------------
// createFilter
// ---------------------------------------------------------------------------

/**
 * @brief Spec: createFilter() returns a non-null shared_ptr with the
 * requested name, columnName, and pattern.
 */
TEST(FilterManagerTest, CreateFilterReturnsCorrectFields)
{
    auto f = FilterManager::getInstance().createFilter(
        "TestCreate", "level", "ERROR");
    ASSERT_NE(f, nullptr);
    EXPECT_EQ(f->name,       "TestCreate");
    EXPECT_EQ(f->columnName, "level");
    EXPECT_EQ(f->pattern,    "ERROR");
}

// ---------------------------------------------------------------------------
// addFilter / getFilters / getFilterByName
// ---------------------------------------------------------------------------

/**
 * @brief Spec: addFilter() appends the filter; getFilters() returns all
 * managed filters including the newly added one.
 */
TEST(FilterManagerTest, AddFilterAppearsInGetFilters)
{
    FilterGuard g;
    const std::string name = "FM_AddTest";
    g.names.push_back(name);

    auto f = std::make_shared<Filter>(name, "level", "ERROR");
    FilterManager::getInstance().addFilter(f);

    const auto& list = FilterManager::getInstance().getFilters();
    const bool found = std::any_of(list.begin(), list.end(),
        [&](const FilterPtr& p) { return p->name == name; });
    EXPECT_TRUE(found);
}

/**
 * @brief Spec: getFilterByName() returns the filter when it exists.
 */
TEST(FilterManagerTest, GetFilterByNameReturnsExistingFilter)
{
    FilterGuard g;
    const std::string name = "FM_GetByName";
    g.names.push_back(name);

    FilterManager::getInstance().addFilter(
        std::make_shared<Filter>(name, "svc", "auth"));

    auto ptr = FilterManager::getInstance().getFilterByName(name);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->name, name);
}

/**
 * @brief Spec: getFilterByName() returns nullptr when the name does not exist.
 */
TEST(FilterManagerTest, GetFilterByNameReturnsNullForUnknown)
{
    auto ptr = FilterManager::getInstance().getFilterByName(
        "DEFINITELY_DOES_NOT_EXIST_XYZ");
    EXPECT_EQ(ptr, nullptr);
}

/**
 * @brief Spec: addFilter() silently rejects a second filter with the same
 * name — the list count must not increase.
 */
TEST(FilterManagerTest, AddFilterRejectsDuplicateName)
{
    FilterGuard g;
    const std::string name = "FM_DupTest";
    g.names.push_back(name);

    FilterManager::getInstance().addFilter(
        std::make_shared<Filter>(name, "level", "ERROR"));

    const size_t countBefore = FilterManager::getInstance().getFilters().size();

    // Add again with the same name but different pattern
    FilterManager::getInstance().addFilter(
        std::make_shared<Filter>(name, "level", "INFO"));

    const size_t countAfter = FilterManager::getInstance().getFilters().size();
    EXPECT_EQ(countBefore, countAfter);
}

// ---------------------------------------------------------------------------
// removeFilter
// ---------------------------------------------------------------------------

/**
 * @brief Spec: removeFilter() removes only the named filter; all others remain.
 */
TEST(FilterManagerTest, RemoveFilterLeavesOthersIntact)
{
    FilterGuard g;
    const std::string nameA = "FM_RemoveA";
    const std::string nameB = "FM_RemoveB";
    g.names.push_back(nameB); // A will be removed inside the test

    FilterManager::getInstance().addFilter(
        std::make_shared<Filter>(nameA, "level", "ERROR"));
    FilterManager::getInstance().addFilter(
        std::make_shared<Filter>(nameB, "level", "INFO"));

    FilterManager::getInstance().removeFilter(nameA);

    EXPECT_EQ(FilterManager::getInstance().getFilterByName(nameA), nullptr);
    EXPECT_NE(FilterManager::getInstance().getFilterByName(nameB), nullptr);
}

// ---------------------------------------------------------------------------
// updateFilter
// ---------------------------------------------------------------------------

/**
 * @brief Spec: updateFilter() replaces an existing filter matched by name.
 * The new filter's pattern must be visible through getFilterByName().
 */
TEST(FilterManagerTest, UpdateFilterReplacesExistingByName)
{
    FilterGuard g;
    const std::string name = "FM_UpdateTest";
    g.names.push_back(name);

    FilterManager::getInstance().addFilter(
        std::make_shared<Filter>(name, "level", "ERROR"));

    auto updated = std::make_shared<Filter>(name, "level", "CRITICAL");
    FilterManager::getInstance().updateFilter(updated);

    auto ptr = FilterManager::getInstance().getFilterByName(name);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->pattern, "CRITICAL");
}

/**
 * @brief Spec: updateFilter() appends the filter when no filter with that
 * name currently exists.
 */
TEST(FilterManagerTest, UpdateFilterAppendsWhenNameIsNew)
{
    FilterGuard g;
    const std::string name = "FM_UpdateNew";
    g.names.push_back(name);

    // Do NOT add beforehand
    FilterManager::getInstance().updateFilter(
        std::make_shared<Filter>(name, "svc", "auth"));

    auto ptr = FilterManager::getInstance().getFilterByName(name);
    ASSERT_NE(ptr, nullptr);
    EXPECT_EQ(ptr->columnName, "svc");
}

// ---------------------------------------------------------------------------
// enableFilter / enableAllFilters
// ---------------------------------------------------------------------------

/**
 * @brief Spec: enableFilter(name, false) disables a filter so applyFilters()
 * no longer uses it — all events pass through.
 */
TEST(FilterManagerTest, DisabledFilterPassesAllEvents)
{
    FilterGuard g;
    const std::string name = "FM_EnableTest";
    g.names.push_back(name);

    // Silence all pre-existing filters so our test filter is the only active one
    FilterManager::getInstance().enableAllFilters(false);

    // Filter that would normally keep only ERROR events
    FilterManager::getInstance().addFilter(
        std::make_shared<Filter>(name, "level", "ERROR"));

    auto container = std::make_unique<db::EventsContainer>();
    FillContainer(*container, {
        {{"level", "ERROR"}, {"msg", "a"}},
        {{"level", "INFO"},  {"msg", "b"}},
        {{"level", "WARN"},  {"msg", "c"}},
    });

    // With only the test filter enabled, only the ERROR event passes
    FilterManager::getInstance().enableFilter(name, true);
    auto withEnabled = FilterManager::getInstance().applyFilters(*container);
    ASSERT_EQ(withEnabled.size(), 1u);

    // Disable the filter — no active filters → all events pass
    FilterManager::getInstance().enableFilter(name, false);
    auto withDisabled = FilterManager::getInstance().applyFilters(*container);
    EXPECT_EQ(withDisabled.size(), container->Size());
}

/**
 * @brief Spec: enableAllFilters(false) disables every filter at once.
 * applyFilters() must then return all indices.
 */
TEST(FilterManagerTest, EnableAllFiltersDisablesAll)
{
    FilterGuard g;
    const std::string nameA = "FM_EnableAll_A";
    const std::string nameB = "FM_EnableAll_B";
    g.names.push_back(nameA);
    g.names.push_back(nameB);

    FilterManager::getInstance().addFilter(
        std::make_shared<Filter>(nameA, "level", "ERROR"));
    FilterManager::getInstance().addFilter(
        std::make_shared<Filter>(nameB, "svc",   "db"));
    FilterManager::getInstance().enableAllFilters(true);

    auto container = std::make_unique<db::EventsContainer>();
    FillContainer(*container, {
        {{"level", "ERROR"}, {"svc", "db"},   {"msg", "1"}},
        {{"level", "INFO"},  {"svc", "auth"}, {"msg", "2"}},
    });

    // With both active, only events matching BOTH filters pass
    const size_t withEnabled =
        FilterManager::getInstance().applyFilters(*container).size();

    FilterManager::getInstance().enableAllFilters(false);
    const size_t withDisabled =
        FilterManager::getInstance().applyFilters(*container).size();

    // Disabling all should return more (or equal) results than both enabled
    EXPECT_GE(withDisabled, withEnabled);
    // Specifically, all events should pass through
    EXPECT_EQ(withDisabled, container->Size());

    // Re-enable for cleanup consistency
    FilterManager::getInstance().enableAllFilters(true);
}

// ---------------------------------------------------------------------------
// applyFilters
// ---------------------------------------------------------------------------

/**
 * @brief Spec: applyFilters() returns indices of events that match ALL enabled
 * filters (intersection / AND across filters).
 */
TEST(FilterManagerTest, ApplyFiltersReturnsMatchingIndices)
{
    FilterGuard g;
    const std::string name = "FM_ApplyTest";
    g.names.push_back(name);

    auto f = std::make_shared<Filter>(name, "level", "ERROR");
    f->compile();
    FilterManager::getInstance().addFilter(f);
    FilterManager::getInstance().enableFilter(name, true);

    auto container = std::make_unique<db::EventsContainer>();
    FillContainer(*container, {
        {{"level", "ERROR"}, {"msg", "bad"}},   // index 0 — should match
        {{"level", "INFO"},  {"msg", "ok"}},    // index 1 — should not match
        {{"level", "ERROR"}, {"msg", "worse"}}, // index 2 — should match
    });

    auto result = FilterManager::getInstance().applyFilters(*container);

    ASSERT_EQ(result.size(), 2u);
    // Indices 0 and 2 must be in the result (order may vary)
    EXPECT_THAT(result, ::testing::UnorderedElementsAre(0u, 2u));
}

/**
 * @brief Spec: when no filters are present (or all disabled), applyFilters()
 * returns all indices.
 */
TEST(FilterManagerTest, NoActiveFiltersReturnsAllIndices)
{
    // Ensure any leftover filters from other tests are disabled
    FilterManager::getInstance().enableAllFilters(false);

    auto container = std::make_unique<db::EventsContainer>();
    FillContainer(*container, {
        {{"level", "ERROR"}},
        {{"level", "INFO"}},
        {{"level", "WARN"}},
    });

    auto result = FilterManager::getInstance().applyFilters(*container);
    EXPECT_EQ(result.size(), container->Size());

    FilterManager::getInstance().enableAllFilters(true);
}

// ---------------------------------------------------------------------------
// Persistence — saveFiltersToPath / loadFiltersFromPath
// ---------------------------------------------------------------------------

/**
 * @brief Spec: saveFiltersToPath() followed by loadFiltersFromPath() from the
 * same path restores the same filters (same names, columns, patterns).
 */
TEST(FilterManagerTest, PersistenceRoundTripPreservesFilters)
{
    const std::string tmpPath =
        (std::filesystem::temp_directory_path() /
         "logviewer_test_filters.json").string();

    // Build a fresh manager state with two known filters
    const std::string nameA = "Persist_A";
    const std::string nameB = "Persist_B";

    FilterManager::getInstance().addFilter(
        std::make_shared<Filter>(nameA, "level", "ERROR"));
    FilterManager::getInstance().addFilter(
        std::make_shared<Filter>(nameB, "svc",   "database"));

    // Save
    auto saveResult = FilterManager::getInstance().saveFiltersToPath(tmpPath);
    ASSERT_TRUE(saveResult.isOk()) << "saveFiltersToPath failed";

    // Clear the in-memory list by removing the two test filters
    FilterManager::getInstance().removeFilter(nameA);
    FilterManager::getInstance().removeFilter(nameB);

    // Load back
    auto loadResult = FilterManager::getInstance().loadFiltersFromPath(tmpPath);
    ASSERT_TRUE(loadResult.isOk()) << "loadFiltersFromPath failed";

    // Verify both filters are present
    auto ptrA = FilterManager::getInstance().getFilterByName(nameA);
    auto ptrB = FilterManager::getInstance().getFilterByName(nameB);
    ASSERT_NE(ptrA, nullptr);
    ASSERT_NE(ptrB, nullptr);
    EXPECT_EQ(ptrA->columnName, "level");
    EXPECT_EQ(ptrA->pattern,    "ERROR");
    EXPECT_EQ(ptrB->columnName, "svc");
    EXPECT_EQ(ptrB->pattern,    "database");

    // Cleanup
    FilterManager::getInstance().removeFilter(nameA);
    FilterManager::getInstance().removeFilter(nameB);
    std::filesystem::remove(tmpPath);
}

/**
 * @brief Spec: loadFiltersFromPath() with a non-existent file returns an error
 * result (not a crash or silent success).
 */
TEST(FilterManagerTest, LoadFromNonExistentPathReturnsError)
{
    auto result = FilterManager::getInstance().loadFiltersFromPath(
        "/tmp/this_file_does_not_exist_xyz_12345.json");

    // The result should hold an error, not success
    EXPECT_TRUE(result.isErr());
}

} // namespace filters
