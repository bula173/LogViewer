#include "FileTailer.hpp"

#include "Logger.hpp"

#include <QFileInfo>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <system_error>

namespace ui::qt
{

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

FileTailer::FileTailer(QObject* parent)
    : QObject(parent)
{
    connect(&m_watcher, &QFileSystemWatcher::fileChanged,
            this,       &FileTailer::OnFileChanged);
}

// ---------------------------------------------------------------------------
// Start / Stop
// ---------------------------------------------------------------------------

void FileTailer::Start(const std::filesystem::path& path,
                       std::unique_ptr<parser::IDataParser> parser,
                       db::EventsContainer& events)
{
    Stop();

    // Reject formats that can't incrementally parse a mid-file stream.
    const std::string ext = [&] {
        std::string e = path.extension().string();
        for (char& c : e) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return e;
    }();

    if (ext == ".xml" || ext == ".csv")
    {
        emit TailingError(
            tr("Tailing is not supported for %1 files (format requires full-file parsing).")
                .arg(QString::fromStdString(ext)));
        return;
    }

    // Seed the offset at the current end of file so we only pick up NEW bytes.
    std::error_code ec;
    const uintmax_t size = std::filesystem::file_size(path, ec);
    if (ec)
    {
        emit TailingError(
            tr("Cannot stat file: %1").arg(QString::fromStdString(ec.message())));
        return;
    }

    m_path       = path;
    m_parser     = std::move(parser);
    m_events     = &events;
    m_lastOffset = size;
    m_active     = true;

    m_observer = std::make_unique<TailObserver>(*m_events);
    m_parser->RegisterObserver(m_observer.get());

    m_watcher.addPath(QString::fromStdString(path.string()));
    util::Logger::Info("[FileTailer] Started watching '{}' at offset {}",
        path.string(), m_lastOffset);
}

void FileTailer::Stop()
{
    if (!m_active) return;

    if (!m_watcher.files().isEmpty())
        m_watcher.removePaths(m_watcher.files());

    if (m_parser && m_observer)
        m_parser->UnregisterObserver(m_observer.get());

    m_parser.reset();
    m_observer.reset();
    m_events     = nullptr;
    m_active     = false;
    m_lastOffset = 0;
    util::Logger::Info("[FileTailer] Stopped");
}

// ---------------------------------------------------------------------------
// File-changed slot
// ---------------------------------------------------------------------------

void FileTailer::OnFileChanged(const QString& changedPath)
{
    if (!m_active || !m_events || !m_parser) return;

    // QFileSystemWatcher may remove the path after a rename/replace.
    // Re-add it so we keep watching (common pattern for log-rotate).
    if (!m_watcher.files().contains(changedPath))
        m_watcher.addPath(changedPath);

    std::error_code ec;
    const uintmax_t newSize = std::filesystem::file_size(m_path, ec);
    if (ec)
    {
        util::Logger::Warn("[FileTailer] Cannot stat '{}': {}", m_path.string(), ec.message());
        return;
    }

    if (newSize < m_lastOffset)
    {
        // File was truncated/rotated — reset to beginning.
        util::Logger::Info("[FileTailer] File truncated, resetting offset to 0");
        m_lastOffset = 0;
    }

    if (newSize == m_lastOffset)
        return; // no new bytes (spurious notification)

    // Read only the new bytes.
    std::ifstream file(m_path, std::ios::binary);
    if (!file.is_open())
    {
        util::Logger::Warn("[FileTailer] Cannot open '{}' for tail read", m_path.string());
        return;
    }

    file.seekg(static_cast<std::streamoff>(m_lastOffset));
    const uintmax_t delta = newSize - m_lastOffset;
    std::string newBytes(static_cast<size_t>(delta), '\0');
    file.read(newBytes.data(), static_cast<std::streamsize>(delta));
    const auto bytesRead = static_cast<uintmax_t>(file.gcount());
    file.close();

    if (bytesRead == 0) return;

    m_lastOffset += bytesRead;

    m_observer->added = 0;

    std::istringstream ss(std::move(newBytes));
    try {
        m_parser->ParseData(ss);
    } catch (const std::exception& ex) {
        util::Logger::Warn("[FileTailer] Parse error on tail read: {}", ex.what());
        emit TailingError(tr("Parse error: %1").arg(QString::fromStdString(ex.what())));
        return;
    }

    if (m_observer->added > 0)
    {
        util::Logger::Debug("[FileTailer] {} new event(s) from tail read", m_observer->added);
        emit NewEventsAvailable(m_observer->added);
    }
}

} // namespace ui::qt
