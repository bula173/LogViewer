// Regression tests for MainWindowPresenter load/merge robustness:
//  - IsParsing() must not stick at true when something throws after parsing
//    (a stuck flag rejects every later load with "already being processed").
//  - The type-filter loops must survive the container being cleared while
//    they yield to the event loop.
//  - MergeLogFile must use the parser chosen by the caller (not the factory's
//    extension guess, which falls back to XML for .log/.txt).
#include <gtest/gtest.h>

#include "ui/MainWindowPresenter.hpp"
#include "csv/CsvParser.hpp"
#include "Config.hpp"
#include "EventsContainer.hpp"

#include <filesystem>
#include <fstream>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

namespace ui::test
{
namespace
{

class FakeView : public IMainWindowView
{
  public:
    std::function<void()> onProcessPendingEvents;
    int processCalls = 0;
    bool progressVisible = false;
    bool searchEnabled = true;

    std::string ReadSearchQuery() const override { return {}; }
    std::string CurrentStatusText() const override { return {}; }
    void UpdateStatusText(const std::string&) override {}
    void SetSearchControlsEnabled(bool enabled) override { searchEnabled = enabled; }
    void ToggleProgressVisibility(bool visible) override { progressVisible = visible; }
    void ConfigureProgressRange(int) override {}
    void UpdateProgressValue(int) override {}
    void ProcessPendingEvents() override
    {
        ++processCalls;
        if (onProcessPendingEvents)
            onProcessPendingEvents();
    }
    void RefreshLayout() override {}
    std::string AskString(const std::string&, const std::string&,
        const std::string&, bool& ok) override
    {
        ok = false;
        return {};
    }
    void UpdateFilterStatus(int, int, int, const std::string&) override {}
};

class FakeController : public mvc::IController
{
  public:
    std::vector<std::string> GetSearchColumns() const override { return {}; }
    void SearchEvents(const std::string&, const std::vector<std::string>&,
        const std::function<void(const mvc::SearchResultRow&)>&,
        std::function<void(size_t, size_t)>) override {}
    void LoadLogFile(const std::filesystem::path&, parser::IDataParserObserver*) override {}
    uint32_t GetParserCurrentProgress() const override { return 0; }
    uint32_t GetParserTotalProgress() const override { return 0; }
};

class FakeSearchResults : public ISearchResultsView
{
  public:
    void SetObserver(ISearchResultsViewObserver*) override {}
    void BeginUpdate(const std::vector<std::string>&) override {}
    void AppendResult(const mvc::SearchResultRow&) override {}
    void EndUpdate() override {}
    void Clear() override {}
};

class FakeEventsList : public IEventsListView
{
  public:
    bool throwOnRefresh = false;
    void RefreshColumns() override {}
    void RefreshView() override
    {
        if (throwOnRefresh)
        {
            throwOnRefresh = false;
            throw std::runtime_error("refresh failed");
        }
    }
    void SetFilteredEvents(const std::vector<unsigned long>&) override {}
    void ClearFilter() override {}
    void UpdateColors() override {}
};

class FakeTypeFilter : public ITypeFilterView
{
  public:
    void SetOnFilterChanged(std::function<void()>) override {}
    void ReplaceTypes(const std::vector<std::string>&, bool) override {}
    void ShowControl(bool) override {}
    void SelectAll() override {}
    void DeselectAll() override {}
    void InvertSelection() override {}
    std::vector<std::string> CheckedTypes() const override { return {}; }
    bool Empty() const override { return true; }
};

} // namespace

class MainWindowPresenterRobustnessTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_savedTypeField = config::GetConfig().typeFilterField;
        config::GetConfig().typeFilterField = "level";
    }

    void TearDown() override
    {
        config::GetConfig().typeFilterField = m_savedTypeField;
        for (const auto& p : m_temps)
        {
            std::error_code ec;
            std::filesystem::remove(p, ec);
        }
    }

    std::filesystem::path WriteCsv(const std::string& name, int rows)
    {
        const auto p = std::filesystem::temp_directory_path()
            / ("MainWindowPresenterRobustnessTest_" + name);
        std::ofstream f(p, std::ios::binary | std::ios::trunc);
        f << "timestamp,level,info\n";
        for (int i = 0; i < rows; ++i)
            f << "2025-01-01T10:00:" << (10 + i % 50) << ","
              << (i % 2 ? "INFO" : "WARN") << ",row " << i << "\n";
        m_temps.push_back(p);
        return p;
    }

    MainWindowPresenter MakePresenter()
    {
        return MainWindowPresenter(m_view, m_controller, m_events, m_search,
            &m_list, &m_typeFilter, nullptr);
    }

    FakeView m_view;
    FakeController m_controller;
    db::EventsContainer m_events;
    FakeSearchResults m_search;
    FakeEventsList m_list;
    FakeTypeFilter m_typeFilter;
    std::string m_savedTypeField;
    std::vector<std::filesystem::path> m_temps;
};

TEST_F(MainWindowPresenterRobustnessTest, ClearDuringTypeFilterScanDoesNotThrowOrStick)
{
    const auto path = WriteCsv("reentrant.csv", 2000);
    auto presenter = MakePresenter();

    // Call 1 is the pump after parsing; call 2 is the first yield inside the
    // UpdateTypeFilters() scan. Simulate a handler clearing the data there.
    m_view.onProcessPendingEvents = [this] {
        if (m_view.processCalls == 2)
            m_events.Clear();
    };

    EXPECT_NO_THROW(presenter.LoadLogFile(std::make_unique<parser::CsvParser>(), path));
    EXPECT_FALSE(presenter.IsParsing());

    // A later load must be accepted.
    m_view.onProcessPendingEvents = nullptr;
    EXPECT_NO_THROW(presenter.LoadLogFile(std::make_unique<parser::CsvParser>(), path));
    EXPECT_EQ(m_events.Size(), 2000u);
}

TEST_F(MainWindowPresenterRobustnessTest, ExceptionAfterParseResetsParsingState)
{
    const auto path = WriteCsv("throwing.csv", 10);
    auto presenter = MakePresenter();

    m_list.throwOnRefresh = true;
    EXPECT_THROW(presenter.LoadLogFile(std::make_unique<parser::CsvParser>(), path),
                 std::runtime_error);

    EXPECT_FALSE(presenter.IsParsing());
    EXPECT_FALSE(m_view.progressVisible);
    EXPECT_TRUE(m_view.searchEnabled);
    EXPECT_NO_THROW(presenter.LoadLogFile(std::make_unique<parser::CsvParser>(), path));
}

TEST_F(MainWindowPresenterRobustnessTest, MergeUsesCallerSuppliedParser)
{
    // A CSV file with a ".log" extension: the factory would pick XmlParser.
    const auto first = WriteCsv("first.csv", 3);
    const auto second = WriteCsv("second.log", 4);
    auto presenter = MakePresenter();

    presenter.LoadLogFile(std::make_unique<parser::CsvParser>(), first);
    ASSERT_EQ(m_events.Size(), 3u);

    presenter.MergeLogFile(std::make_unique<parser::CsvParser>(), second, "a", "b");

    EXPECT_EQ(m_events.Size(), 7u);
    EXPECT_FALSE(presenter.IsParsing());
}

} // namespace ui::test
