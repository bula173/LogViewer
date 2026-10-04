#pragma once

#include "analyzers/TestOutline.hpp"

#include <QWidget>

#include <cstddef>
#include <cstdint>

class QFrame;
class QLabel;
class QPushButton;
class QTreeWidget;
class QTreeWidgetItem;

namespace db {
class EventsContainer;
}

namespace ui::qt {

/**
 * @brief "Test Steps" tab: the step outline of a test-harness log.
 *
 * Shows analyzer::BuildTestOutline() as a tree (sections → keywords and
 * expectations) with status, start relative to TEST_START, duration and event
 * count, under a banner with the verdict and, for a failed test, the step and
 * message that failed it plus a "Go to failure" button. Double-click or Enter
 * on a row, or the button, emits NavigateToEvent() with the event's container
 * index. Logs without TEST_START only get a short hint.
 *
 * The outline describes the whole log, not the filtered rows, so Refresh()
 * re-analyses only when the loaded data changes.
 */
class TestStepsPanel : public QWidget
{
    Q_OBJECT

  public:
    explicit TestStepsPanel(db::EventsContainer& events, QWidget* parent = nullptr);

  public slots:
    /// Re-analyses the events if they changed since the last call.
    void Refresh();

  signals:
    void NavigateToEvent(int actualRow);

  protected:
    /// Enter / Return on the tree activates the current row.
    bool eventFilter(QObject* watched, QEvent* event) override;

  private:
    void BuildLayout();
    void ShowOutline();
    void ActivateItem(QTreeWidgetItem* item);

    db::EventsContainer&  m_events;
    analyzer::TestOutline m_outline;

    // Data the outline was built from (see Refresh()).
    bool          m_built {false};
    std::uint64_t m_builtGeneration {0};
    std::size_t   m_builtSize {0};
    db::EventsContainer::FileMetadata m_builtMetadata;

    QLabel*      m_hint        {nullptr};
    QFrame*      m_banner      {nullptr};
    QLabel*      m_bannerText  {nullptr};
    QPushButton* m_goToFailure {nullptr};
    QTreeWidget* m_tree        {nullptr};
};

} // namespace ui::qt
