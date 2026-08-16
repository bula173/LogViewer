#include <gtest/gtest.h>

#include "ui/qt/utils/FileTailer.hpp"
#include "json/JsonParser.hpp"
#include "Error.hpp"
#include "EventsContainer.hpp"

#include <QApplication>

#include <filesystem>
#include <fstream>
#include <string>

// ---------------------------------------------------------------------------
// Ensure a QApplication exists for the whole test binary (required by Qt).
// Must be QApplication, not QCoreApplication: other test files in this binary
// (e.g. FilterProfilesPanelTest) construct real QWidgets, and only one
// Q(Core)Application may exist per process — whichever test's GetApp()/
// EnsureQApplication() runs first wins for the rest of the binary's lifetime.
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// Utilities
// ---------------------------------------------------------------------------

static void WriteFile(const std::filesystem::path& p, const std::string& s)
{
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f << s;
}

static void AppendFile(const std::filesystem::path& p, const std::string& s)
{
    std::ofstream f(p, std::ios::binary | std::ios::app);
    f << s;
}

static std::filesystem::path TempPath(const std::string& name)
{
    return std::filesystem::temp_directory_path() / ("FileTailerTest_" + name);
}

// ---------------------------------------------------------------------------
// Fixture
// ---------------------------------------------------------------------------

class FileTailerTest : public ::testing::Test
{
protected:
    void SetUp()    override { GetApp(); }
    void TearDown() override
    {
        for (const auto& p : m_temps)
        {
            std::error_code ec;
            std::filesystem::remove(p, ec);
        }
    }

    std::filesystem::path MakeTemp(const std::string& name,
                                   const std::string& content = "")
    {
        auto p = TempPath(name);
        WriteFile(p, content);
        m_temps.push_back(p);
        return p;
    }

    // Invoke the private OnFileChanged slot directly.
    static void FireChanged(FileTailer& t, const std::filesystem::path& p)
    {
        QMetaObject::invokeMethod(&t, "OnFileChanged",
            Qt::DirectConnection,
            Q_ARG(QString, QString::fromStdString(p.string())));
    }

    std::vector<std::filesystem::path> m_temps;
};

// ---------------------------------------------------------------------------
// Tests
// ---------------------------------------------------------------------------

TEST_F(FileTailerTest, StartsInactive)
{
    FileTailer t;
    EXPECT_FALSE(t.IsActive());
}

TEST_F(FileTailerTest, StartStop)
{
    auto p = MakeTemp("start_stop.jsonl", "{\"a\":\"1\"}\n");
    db::EventsContainer ev;
    FileTailer t;
    t.Start(p, std::make_unique<parser::JsonParser>(), ev);
    EXPECT_TRUE(t.IsActive());
    t.Stop();
    EXPECT_FALSE(t.IsActive());
}

TEST_F(FileTailerTest, StopWhileInactiveIsNoop)
{
    FileTailer t;
    EXPECT_NO_THROW(t.Stop());
}

TEST_F(FileTailerTest, UnsupportedXmlEmitsErrorAndStaysInactive)
{
    auto p = MakeTemp("test.xml", "<events/>");
    db::EventsContainer ev;
    FileTailer t;

    int errCount = 0;
    QObject::connect(&t, &FileTailer::TailingError,
                     [&errCount](const QString&){ ++errCount; });

    t.Start(p, std::make_unique<parser::JsonParser>(), ev);
    EXPECT_FALSE(t.IsActive());
    EXPECT_EQ(errCount, 1);
}

TEST_F(FileTailerTest, UnsupportedCsvEmitsError)
{
    auto p = MakeTemp("test.csv", "a,b\n1,2\n");
    db::EventsContainer ev;
    FileTailer t;

    int errCount = 0;
    QObject::connect(&t, &FileTailer::TailingError,
                     [&errCount](const QString&){ ++errCount; });

    t.Start(p, std::make_unique<parser::JsonParser>(), ev);
    EXPECT_FALSE(t.IsActive());
    EXPECT_GE(errCount, 1);
}

TEST_F(FileTailerTest, NewLineAddsEvent)
{
    auto p = MakeTemp("new_line.jsonl", "{\"x\":\"old\"}\n");
    db::EventsContainer ev;
    FileTailer t;

    int newEvCount = 0;
    QObject::connect(&t, &FileTailer::NewEventsAvailable,
        [&newEvCount](std::size_t n){ newEvCount += static_cast<int>(n); });

    t.Start(p, std::make_unique<parser::JsonParser>(), ev);
    ASSERT_TRUE(t.IsActive());

    AppendFile(p, "{\"x\":\"new\"}\n");
    FireChanged(t, p);

    EXPECT_EQ(newEvCount, 1);
    ASSERT_EQ(ev.Size(), 1u);
    EXPECT_EQ(ev.GetEvent(0).findByKey("x"), "new");
}

TEST_F(FileTailerTest, MultipleNewLines)
{
    auto p = MakeTemp("multi.jsonl", "");
    db::EventsContainer ev;
    FileTailer t;

    int total = 0;
    QObject::connect(&t, &FileTailer::NewEventsAvailable,
        [&total](std::size_t n){ total += static_cast<int>(n); });

    t.Start(p, std::make_unique<parser::JsonParser>(), ev);
    AppendFile(p, "{\"n\":\"1\"}\n{\"n\":\"2\"}\n{\"n\":\"3\"}\n");
    FireChanged(t, p);

    EXPECT_EQ(total, 3);
    EXPECT_EQ(ev.Size(), 3u);
}

TEST_F(FileTailerTest, NoNewBytesNoSignal)
{
    auto p = MakeTemp("noop.jsonl", "{\"a\":\"1\"}\n");
    db::EventsContainer ev;
    FileTailer t;

    int fired = 0;
    QObject::connect(&t, &FileTailer::NewEventsAvailable,
        [&fired](std::size_t){ ++fired; });

    t.Start(p, std::make_unique<parser::JsonParser>(), ev);
    FireChanged(t, p); // no new bytes

    EXPECT_EQ(fired, 0);
    EXPECT_EQ(ev.Size(), 0u);
}

TEST_F(FileTailerTest, FileTruncationResetsOffset)
{
    auto p = MakeTemp("trunc.jsonl", "{\"a\":\"old1\"}\n{\"a\":\"old2\"}\n");
    db::EventsContainer ev;
    FileTailer t;
    t.Start(p, std::make_unique<parser::JsonParser>(), ev);
    ASSERT_TRUE(t.IsActive());

    // Simulate log rotation.
    WriteFile(p, "{\"a\":\"rotated\"}\n");
    FireChanged(t, p);

    ASSERT_EQ(ev.Size(), 1u);
    EXPECT_EQ(ev.GetEvent(0).findByKey("a"), "rotated");
}

TEST_F(FileTailerTest, ExistingContentNotReplayed)
{
    // Tailer seeds offset at EOF on Start; pre-existing lines must not appear.
    auto p = MakeTemp("existing.jsonl",
        "{\"x\":\"pre1\"}\n{\"x\":\"pre2\"}\n");
    db::EventsContainer ev;
    FileTailer t;
    t.Start(p, std::make_unique<parser::JsonParser>(), ev);
    FireChanged(t, p); // no new bytes

    EXPECT_EQ(ev.Size(), 0u);
}

TEST_F(FileTailerTest, SecondStartReplacesPreviousWatcher)
{
    auto p1 = MakeTemp("restart1.jsonl", "{\"v\":\"a\"}\n");
    auto p2 = MakeTemp("restart2.jsonl", "");
    db::EventsContainer ev;
    FileTailer t;
    t.Start(p1, std::make_unique<parser::JsonParser>(), ev);
    EXPECT_TRUE(t.IsActive());

    t.Start(p2, std::make_unique<parser::JsonParser>(), ev);
    EXPECT_TRUE(t.IsActive());
    EXPECT_EQ(t.FilePath(), p2);

    AppendFile(p2, "{\"v\":\"b\"}\n");
    FireChanged(t, p2);
    ASSERT_EQ(ev.Size(), 1u);
    EXPECT_EQ(ev.GetEvent(0).findByKey("v"), "b");
}

} // namespace ui::qt::test
