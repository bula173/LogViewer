#pragma once

#include "EventsContainer.hpp"
#include "IDataParser.hpp"

#include <QFileSystemWatcher>
#include <QObject>
#include <QString>

#include <cstdint>
#include <filesystem>
#include <memory>

namespace ui::qt
{

/// Watches a log file for new content and incrementally parses appended bytes.
///
/// After construction call Start(); the tailer attaches a QFileSystemWatcher
/// to the given path. Each time the file grows the new bytes are extracted and
/// fed to the parser's ParseData(istream&) overload. The supplied
/// EventsContainer receives the new events via the parser's observer mechanism.
///
/// **Supported formats** (line- or record-based parsers that accept a partial
/// stream starting mid-file):
///   NDJSON/JSONL, CAN/ASC, AUTOSAR DLT
///
/// **Unsupported formats**: JSON array (whole-document), XML (stateful), CSV
/// (header in first line). For these formats Start() will emit TailingError.
///
/// The parser instance is owned externally and must outlive FileTailer.
class FileTailer : public QObject
{
    Q_OBJECT

public:
    explicit FileTailer(QObject* parent = nullptr);
    ~FileTailer() override = default;

    /// Begin watching @p path, parsing new bytes with @p parser into @p events.
    /// Emits TailingError immediately if the format is known-unsupported.
    void Start(const std::filesystem::path& path,
               std::unique_ptr<parser::IDataParser> parser,
               db::EventsContainer& events);

    void Stop();
    bool IsActive() const { return m_active; }

    const std::filesystem::path& FilePath() const { return m_path; }

signals:
    /// Emitted on the main thread after new events have been added.
    void NewEventsAvailable(std::size_t count);

    /// Emitted when a tailing read or parse error occurs.
    void TailingError(const QString& message);

private slots:
    void OnFileChanged(const QString& path);

private:
    struct TailObserver final : parser::IDataParserObserver
    {
        db::EventsContainer& container;
        std::size_t          added {0};

        explicit TailObserver(db::EventsContainer& c) : container(c) {}

        void ProgressUpdated() override {}
        void NewEventFound(db::LogEvent&& ev) override
        {
            container.AddEvent(std::move(ev));
            ++added;
        }
        void NewEventBatchFound(
            std::vector<std::pair<int, db::LogEvent::EventItems>>&& batch) override
        {
            for (auto& [id, items] : batch)
            {
                container.AddEvent(db::LogEvent(id, std::move(items)));
                ++added;
            }
        }
    };

    QFileSystemWatcher                   m_watcher;
    std::unique_ptr<parser::IDataParser> m_parser;
    std::unique_ptr<TailObserver>        m_observer;
    db::EventsContainer*                 m_events  {nullptr};
    std::filesystem::path                m_path;
    uintmax_t                            m_lastOffset {0};
    bool                                 m_active  {false};
};

} // namespace ui::qt
