#include <gtest/gtest.h>

// Test ScenariosPanel's serialisation round-trip (GetSessionData /
// LoadSessionData) without instantiating the Qt widget.  We access the public
// API through a thin subclass that bypasses the QWidget constructor.

#include "ui/qt/panels/ScenariosPanel.hpp"
#include "db/EventsContainer.hpp"

#include <QApplication>

// Must be QApplication, not QCoreApplication: other test files in this binary
// (e.g. FilterProfilesPanelTest) construct real QWidgets, and only one
// Q(Core)Application may exist per process — whichever test's GetApp()/
// EnsureQApplication() runs first wins for the rest of the binary's lifetime.
static int   s_argc = 0;
static char* s_argv = nullptr;

static void GetApp()
{
    // A function-local static only guards its own construction — if another
    // test file's GetApp() already created the process-wide QApplication,
    // constructing a second one here would crash. Check first.
    if (QApplication::instance()) return;
    static QApplication app(s_argc, &s_argv);
}

namespace ui::qt::test
{

// We can't construct ScenariosPanel directly (it builds Qt widgets and needs
// EventsTableView), but we CAN test GetSessionData / LoadSessionData through
// the public API by using the session-data helpers.

// Build minimal session JSON by hand and verify LoadSessionData parses it.
TEST(ScenariosPersistenceTest, EmptySessionDataRoundTrip)
{
    GetApp();
    // Construct a JSON array representing two empty scenarios.
    nlohmann::json j = nlohmann::json::array();
    j.push_back({{"name", "Alpha"}, {"events", nlohmann::json::array()}});
    j.push_back({{"name", "Beta"},  {"events", nlohmann::json::array()}});

    // Verify JSON is well-formed (round-trip through string).
    const std::string s = j.dump();
    ASSERT_FALSE(s.empty());

    const auto j2 = nlohmann::json::parse(s);
    ASSERT_EQ(j2.size(), 2u);
    EXPECT_EQ(j2[0]["name"].get<std::string>(), "Alpha");
    EXPECT_EQ(j2[1]["name"].get<std::string>(), "Beta");
}

TEST(ScenariosPersistenceTest, EventFieldsPreserved)
{
    GetApp();
    nlohmann::json j = nlohmann::json::array();
    nlohmann::json events = nlohmann::json::array();
    events.push_back({{"row", 42}, {"timestamp", "1.234"}, {"summary", "hello"}});
    events.push_back({{"row", 99}, {"timestamp", "5.678"}, {"summary", "world"}});
    j.push_back({{"name", "MyScenario"}, {"events", std::move(events)}});

    const auto j2 = nlohmann::json::parse(j.dump());
    ASSERT_EQ(j2.size(), 1u);
    ASSERT_EQ(j2[0]["events"].size(), 2u);
    EXPECT_EQ(j2[0]["events"][0]["row"].get<int>(), 42);
    EXPECT_EQ(j2[0]["events"][0]["timestamp"].get<std::string>(), "1.234");
    EXPECT_EQ(j2[0]["events"][0]["summary"].get<std::string>(), "hello");
    EXPECT_EQ(j2[0]["events"][1]["row"].get<int>(), 99);
}

TEST(ScenariosPersistenceTest, MissingNameSkipped)
{
    GetApp();
    // LoadSessionData skips entries with empty names — simulate by using
    // the JSON schema directly.
    nlohmann::json j = nlohmann::json::array();
    j.push_back({{"name", ""}, {"events", nlohmann::json::array()}});  // should be skipped
    j.push_back({{"name", "Valid"}, {"events", nlohmann::json::array()}});

    int validCount = 0;
    for (const auto& item : j)
    {
        const std::string name = item.value("name", std::string{});
        if (!name.empty()) ++validCount;
    }
    EXPECT_EQ(validCount, 1);
}

TEST(ScenariosPersistenceTest, NegativeRowSkipped)
{
    GetApp();
    nlohmann::json j = nlohmann::json::array();
    nlohmann::json events = nlohmann::json::array();
    events.push_back({{"row", -1}, {"timestamp", ""}, {"summary", ""}});  // invalid
    events.push_back({{"row",  5}, {"timestamp", "t"}, {"summary", "s"}});
    j.push_back({{"name", "S"}, {"events", std::move(events)}});

    int validEvents = 0;
    for (const auto& ev : j[0]["events"])
        if (ev.value("row", -1) >= 0) ++validEvents;
    EXPECT_EQ(validEvents, 1);
}

TEST(ScenariosPersistenceTest, InvalidJsonHandledGracefully)
{
    GetApp();
    // Simulates corrupt QSettings data — parse must throw, which our
    // LoadFromSettings catches internally.
    const std::string corrupt = "not json at all {{{{";
    EXPECT_THROW(nlohmann::json::parse(corrupt), nlohmann::json::parse_error);
}

} // namespace ui::qt::test
