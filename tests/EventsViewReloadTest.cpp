// The events table view must show rows after a reload that goes through the
// same container notifications the presenter uses.
#include <gtest/gtest.h>
#include <QApplication>

#include "qt/events/EventsTableView.hpp"
#include "qt/events/EventsTableModel.hpp"
#include "EventsContainer.hpp"
#include "Config.hpp"
#include "parsers/sapi/SapiLogParser.hpp"
#include <filesystem>

namespace ui::qt::test {

namespace {
void EnsureQApplication()
{
    if (QApplication::instance()) return;
    static int argc = 1;
    static char argv0[] = "tests";
    static char* argv[] = {argv0};
    static QApplication app(argc, argv);
}
} // namespace

TEST(EventsViewReloadTest, ViewShowsRowsLoadedAfterStartup)
{
    EnsureQApplication();
    const auto saved = config::GetConfig().GetColumns();
    config::GetConfig().GetMutableColumns() = {{"id", true, 50}, {"level", true, 80}};

    db::EventsContainer events;
    EventsTableView view(events);
    view.show();

    events.SuspendNotifications();
    std::vector<std::pair<int, db::LogEvent::EventItems>> batch;
    for (int i = 1; i <= 25; ++i)
        batch.push_back({i, {{"level", i % 2 ? "INFO" : "ERROR"}}});
    events.AddEventBatch(std::move(batch));
    events.ResumeNotifications();
    view.RefreshView();

    EXPECT_EQ(view.model()->rowCount(), 25);
    config::GetConfig().GetMutableColumns() = saved;
}

TEST(EventsViewReloadTest, ColumnFilterFromThePreviousLogDoesNotHideANewLog)
{
    EnsureQApplication();
    const auto saved = config::GetConfig().GetColumns();
    config::GetConfig().GetMutableColumns() = {{"id", true, 50}, {"level", true, 80}};

    db::EventsContainer events;
    EventsTableView view(events);
    view.show();
    auto load = [&](std::vector<const char*> levels) {
        events.SuspendNotifications();
        events.Clear();
        std::vector<std::pair<int, db::LogEvent::EventItems>> batch;
        for (std::size_t i = 0; i < levels.size(); ++i)
            batch.push_back({static_cast<int>(i + 1), {{"level", levels[i]}}});
        events.AddEventBatch(std::move(batch));
        events.ResumeNotifications();
        view.ClearFilter();     // what RunParserAsync does after parsing
        view.RefreshView();
    };

    load({"INFO", "ERROR", "INFO"});
    static_cast<EventsTableModel*>(view.model())->SetColumnFilter(1, {"ERROR"});  // user filters the level column
    load({"DEBUG", "DEBUG", "WARN"});    // new log, no ClearColumnFilters() call

    EXPECT_EQ(view.model()->rowCount(), 3);
    config::GetConfig().GetMutableColumns() = saved;
}

TEST(EventsViewReloadTest, SapiSampleLoadedThroughTheParserShowsAllRows)
{
    EnsureQApplication();
    const auto saved = config::GetConfig().GetColumns();
    config::GetConfig().GetMutableColumns() = {{"timestamp", true, 80}, {"source", true, 80}, {"level", true, 60}};

    db::EventsContainer events;
    EventsTableView view(events);
    view.show();
    view.model()->sort(0, Qt::AscendingOrder);  // what QTableView does with sorting enabled

    parser::SapiLogParser parser;
    struct Obs : parser::IDataParserObserver {
        db::EventsContainer& ev;
        explicit Obs(db::EventsContainer& e) : ev(e) {}
        void ProgressUpdated() override {}
        void NewEventFound(db::LogEvent&& e) override { ev.AddEvent(std::move(e)); }
        void NewEventBatchFound(std::vector<std::pair<int, db::LogEvent::EventItems>>&& b) override { ev.AddEventBatch(std::move(b)); }
    } obs(events);
    parser.RegisterObserver(&obs);
    events.SuspendNotifications();
    parser.ParseData(std::filesystem::path(LOGVIEWER_TEST_DATA_DIR) / "sapi_sample.txt");
    events.ResumeNotifications();
    parser.UnregisterObserver(&obs);
    view.ClearFilter();
    view.RefreshView();

    EXPECT_EQ(events.Size(), 12u);
    EXPECT_EQ(view.model()->rowCount(), 12);
    config::GetConfig().GetMutableColumns() = saved;
}

} // namespace ui::qt::test
