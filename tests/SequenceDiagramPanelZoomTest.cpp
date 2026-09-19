#include <gtest/gtest.h>
#include <QApplication>
#include <QGraphicsScene>
#include <QGraphicsView>
#include <QToolButton>
#include <QTest>
#include <QWheelEvent>

#include "qt/panels/SequenceDiagramPanel.hpp"
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

class SequenceDiagramPanelZoomTest : public ::testing::Test
{
protected:
    void SetUp() override
    {
        EnsureQApplication();
        m_panel = std::make_unique<SequenceDiagramPanel>(m_events, nullptr);
        m_view  = m_panel->findChild<QGraphicsView*>("seqDiagramView");
        ASSERT_NE(m_view, nullptr);
    }

    void Click(const char* name)
    {
        auto* b = m_panel->findChild<QToolButton*>(name);
        ASSERT_NE(b, nullptr) << name;
        b->click();
    }

    qreal Zoom() const { return m_view->transform().m11(); }

    void Wheel(int angleDeltaY, Qt::KeyboardModifiers mods)
    {
        const QPoint pos = m_view->viewport()->rect().center();
        QWheelEvent ev(QPointF(pos), QPointF(m_view->viewport()->mapToGlobal(pos)),
                       QPoint(), QPoint(0, angleDeltaY), Qt::NoButton, mods,
                       Qt::NoScrollPhase, false);
        QApplication::sendEvent(m_view->viewport(), &ev);
    }

    db::EventsContainer m_events;
    std::unique_ptr<SequenceDiagramPanel> m_panel;
    QGraphicsView* m_view {nullptr};
};

TEST_F(SequenceDiagramPanelZoomTest, ZoomButtonsScaleAndReset)
{
    EXPECT_DOUBLE_EQ(Zoom(), 1.0);

    Click("seqZoomInButton");
    EXPECT_NEAR(Zoom(), 1.25, 1e-9);

    Click("seqZoomOutButton");
    Click("seqZoomOutButton");
    EXPECT_NEAR(Zoom(), 0.8, 1e-9);

    Click("seqZoom100Button");
    EXPECT_DOUBLE_EQ(Zoom(), 1.0);
}

TEST_F(SequenceDiagramPanelZoomTest, ZoomIsClamped)
{
    for (int i = 0; i < 40; ++i) Click("seqZoomInButton");
    EXPECT_NEAR(Zoom(), 5.0, 1e-9);

    for (int i = 0; i < 80; ++i) Click("seqZoomOutButton");
    EXPECT_NEAR(Zoom(), 0.05, 1e-9);
}

TEST_F(SequenceDiagramPanelZoomTest, CtrlWheelZoomsPlainWheelDoesNot)
{
    Wheel(120, Qt::NoModifier);
    EXPECT_DOUBLE_EQ(Zoom(), 1.0);

    Wheel(120, Qt::ControlModifier);
    EXPECT_GT(Zoom(), 1.1);

    const qreal zoomed = Zoom();
    Wheel(-120, Qt::ControlModifier);
    EXPECT_LT(Zoom(), zoomed);
}

TEST_F(SequenceDiagramPanelZoomTest, FitWidthShrinksWideSceneButNeverEnlarges)
{
    m_panel->resize(600, 400);
    m_panel->show();
    ASSERT_TRUE(QTest::qWaitForWindowExposed(m_panel.get()));

    m_view->scene()->addRect(0, 0, 3000, 200);
    m_view->scene()->setSceneRect(0, 0, 3000, 200);
    Click("seqFitWidthButton");
    EXPECT_LT(Zoom(), 0.25);
    EXPECT_NEAR(Zoom(), m_view->viewport()->width() / 3000.0, 1e-6);

    m_view->scene()->setSceneRect(0, 0, 100, 100);
    Click("seqFitWidthButton");
    EXPECT_DOUBLE_EQ(Zoom(), 1.0);
}

} // namespace ui::qt::test
