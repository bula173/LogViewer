#pragma once

#include "EventsContainer.hpp"
#include "IDataParser.hpp"

#include <QFutureWatcher>
#include <QLabel>
#include <QString>
#include <QWidget>
#include <filesystem>
#include <memory>

class QComboBox;
class QPushButton;
class QSplitter;

namespace ui::qt
{

class EventsTableView;

/// Panel that displays two independently-loaded log files side by side, with
/// three synchronisation modes:
///
///   Timestamp — when the user selects a row on one side the other side scrolls
///               to the event with the nearest timestamp.
///
///   Manual    — the user marks one reference event on each side; the panel
///               computes a constant timestamp offset between the two logs and
///               from then on uses that offset for the nearest-timestamp lookup.
///               Useful when the two files use incompatible time bases (e.g.
///               DLT epoch vs ASC elapsed seconds).
///
///   None      — the two views are completely independent.
class SideBySidePanel : public QWidget
{
    Q_OBJECT

public:
    explicit SideBySidePanel(QWidget* parent = nullptr);
    ~SideBySidePanel() override;

    void OpenLeft(const QString& path);
    void OpenRight(const QString& path);

signals:
    void CloseRequested();

private slots:
    void OnOpenLeftFile();
    void OnOpenRightFile();
    void OnSyncModeChanged(int index);
    void OnSetLeftRef();
    void OnSetRightRef();
    void OnLeftSelectionChanged(int actualRow);
    void OnRightSelectionChanged(int actualRow);
    void OnLeftLoadFinished();
    void OnRightLoadFinished();

private:
    enum class SyncMode { Timestamp, Manual, None };

    // ── UI helpers ─────────────────────────────────────────────────────────
    void BuildUi();
    QWidget* BuildSide(bool isLeft);
    void UpdateManualControls();
    void UpdateSyncStatus();

    // ── Loading ─────────────────────────────────────────────────────────────
    void LoadFile(bool isLeft);
    void LoadFileDirect(bool isLeft, const QString& path);
    void StartAsyncLoad(bool isLeft, std::unique_ptr<parser::IDataParser> parser,
                        const std::filesystem::path& path);

    // ── Sync helpers ────────────────────────────────────────────────────────
    double GetTimestamp(db::EventsContainer& container, size_t actualRow) const;

    /// Returns the row in @p container whose timestamp is closest to @p target.
    /// Falls back to a row-offset lookup when timestamps are absent.
    int FindNearestByTimestamp(db::EventsContainer& container,
                               double target, double fallbackDelta) const;

    void SyncFromLeft(int actualRow);
    void SyncFromRight(int actualRow);

    // ── Data ────────────────────────────────────────────────────────────────
    db::EventsContainer m_leftEvents;
    db::EventsContainer m_rightEvents;

    EventsTableView* m_leftView   {nullptr};
    EventsTableView* m_rightView  {nullptr};

    QLabel*      m_leftFileLabel  {nullptr};
    QLabel*      m_rightFileLabel {nullptr};
    QPushButton* m_leftOpenBtn    {nullptr};
    QPushButton* m_rightOpenBtn   {nullptr};

    // Sync toolbar
    QComboBox*   m_syncModeCombo  {nullptr};
    QPushButton* m_setLeftRefBtn  {nullptr};
    QPushButton* m_setRightRefBtn {nullptr};
    QLabel*      m_syncStatusLabel{nullptr};

    // ── Sync state ──────────────────────────────────────────────────────────
    SyncMode m_syncMode   {SyncMode::Timestamp};
    bool     m_leftRefSet {false};
    bool     m_rightRefSet{false};
    double   m_leftRefTs  {0.0};   // timestamp of chosen left reference event
    double   m_rightRefTs {0.0};   // timestamp of chosen right reference event
    int      m_leftRefRow {-1};    // actual index in left container
    int      m_rightRefRow{-1};    // actual index in right container
    double   m_syncOffset {0.0};   // rightTs = leftTs + m_syncOffset
    bool     m_syncInProgress{false};

    // ── Async loading ───────────────────────────────────────────────────────
    // The observer and parser must outlive the background future; store them
    // per-side until the QFutureWatcher signals completion.
    struct LoadJob
    {
        std::unique_ptr<parser::IDataParser> parser;

        struct Observer final : parser::IDataParserObserver
        {
            db::EventsContainer& container;
            std::exception_ptr   exception;
            explicit Observer(db::EventsContainer& c) : container(c) {}
            void ProgressUpdated() override {}
            void NewEventFound(db::LogEvent&& ev) override
                { container.AddEvent(std::move(ev)); }
            void NewEventBatchFound(
                std::vector<std::pair<int,db::LogEvent::EventItems>>&& b) override
                { container.AddEventBatch(std::move(b)); }
        };
        std::unique_ptr<Observer> observer;
    };

    LoadJob  m_leftJob;
    LoadJob  m_rightJob;
    QFutureWatcher<void>* m_leftWatcher  {nullptr};
    QFutureWatcher<void>* m_rightWatcher {nullptr};
};

} // namespace ui::qt
