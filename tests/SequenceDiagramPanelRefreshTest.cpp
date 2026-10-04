// Sequence diagram re-discovery: a refresh requested while discovery runs is
// not lost, an unchanged diagram keeps its zoom, and clearing the container
// while discovery runs is safe.
#include <gtest/gtest.h>
#include <QApplication>
#include <QGraphicsView>
#include <QLabel>
#include <QPushButton>
#include <QTest>
#include <QThread>
#include <QToolButton>

#include "qt/panels/SequenceDiagramPanel.hpp"
#include "qt/panels/ActorDefinition.hpp"
#include "EventsContainer.hpp"

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

class SequenceDiagramPanelRefreshTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EnsureQApplication();
        m_panel = std::make_unique<SequenceDiagramPanel>(m_events, nullptr);
        m_view  = m_panel->findChild<QGraphicsView*>("seqDiagramView");
        ASSERT_NE(m_view, nullptr);
    }

    // Appends @p count messages; even ones a → b, odd ones b → a.
    void Append(int count, const char* a, const char* b)
    {
        std::vector<std::pair<int, db::LogEvent::EventItems>> batch;
        batch.reserve(static_cast<std::size_t>(count));
        const int base = static_cast<int>(m_events.Size());
        for (int i = 0; i < count; ++i)
            batch.push_back({base + i, {{"from", i % 2 ? b : a},
                                        {"to",   i % 2 ? a : b},
                                        {"msg",  "ping"}}});
        m_events.AddEventBatch(std::move(batch));
    }

    QString Status() const
    {
        // The status label is the one that shows the discovery result.
        for (auto* l : m_panel->findChildren<QLabel*>())
            if (l->text().contains("Detected") || l->text().contains("Discovering")
                || l->text().contains("No "))
                return l->text();
        return {};
    }
    bool WaitForStatus(const QString& part, int timeoutMs = 30000)
    {
        return QTest::qWaitFor([&] { return Status().contains(part); }, timeoutMs);
    }
    bool WaitIdle()
    {
        auto* btn = m_panel->findChild<QPushButton*>();
        return QTest::qWaitFor([&] {
            return btn->isEnabled() && !Status().contains("Discovering");
        }, 30000);
    }
    qreal Zoom() const { return m_view->transform().m11(); }

    db::EventsContainer m_events;
    std::unique_ptr<SequenceDiagramPanel> m_panel;
    QGraphicsView* m_view {nullptr};
};

TEST_F(SequenceDiagramPanelRefreshTest, RediscoveryOfTheSameActorsKeepsTheZoom)
{
    m_panel->resize(600, 400);
    m_panel->show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(m_panel.get()));

    Append(20, "A", "B");
    m_panel->Refresh();
    ASSERT_TRUE(WaitIdle());
    ASSERT_TRUE(Status().contains("Detected")) << Status().toStdString();

    m_panel->findChild<QToolButton*>("seqZoomInButton")->click();
    const qreal zoomed = Zoom();

    Append(20, "A", "B"); // e.g. tailing: more of the same exchange
    m_panel->Refresh();
    ASSERT_TRUE(WaitIdle());
    EXPECT_DOUBLE_EQ(Zoom(), zoomed);
}

TEST_F(SequenceDiagramPanelRefreshTest, RefreshDuringDiscoveryIsNotLost)
{
    // "C" is the self actor: it gets a lifeline once discovery has seen it,
    // even though its messages are beyond the "Show first" limit.
    ActorDefinition self;
    self.name   = "C";
    self.isSelf = true;
    m_panel->SetDefinitions({self});

    Append(200000, "A", "B"); // large enough for discovery to take a while
    m_panel->Refresh();
    QThread::msleep(100);     // the background scan has started (and runs for a while)
    Append(4, "A", "C");      // new data arrives while discovery runs
    m_panel->Refresh();       // must not be dropped

    EXPECT_TRUE(WaitForStatus("3 actor(s)")) << Status().toStdString();
    EXPECT_TRUE(WaitIdle());
}

TEST_F(SequenceDiagramPanelRefreshTest, ClearWhileDiscoveryRunsIsSafe)
{
    Append(200000, "A", "B");
    m_panel->Refresh();
    m_events.Clear(); // waits for the background scan instead of freeing its events
    EXPECT_EQ(m_events.Size(), 0u);
    Append(20, "X", "Y");
    m_panel->Refresh();
    ASSERT_TRUE(WaitIdle());
    EXPECT_TRUE(Status().contains("Detected")) << Status().toStdString();
}

} // namespace ui::qt::test
