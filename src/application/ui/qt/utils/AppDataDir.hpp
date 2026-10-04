#pragma once

#include "Config.hpp"
#include "PortableMode.hpp"

#include <QSettings>
#include <QStandardPaths>
#include <QString>

#include <filesystem>
#include <system_error>

namespace ui::qt::utils
{

/// Lossless path -> QString. path::string() converts to the ANSI code page on
/// Windows, which garbles or throws on non-ASCII user / folder names.
inline QString PathToQString(const std::filesystem::path& path)
{
    return QString::fromStdU16String(path.u16string());
}

/// Per-user data directory: the `data/` folder next to the executable in
/// portable mode, otherwise Qt's standard location @p location.
inline QString AppDataDir(QStandardPaths::StandardLocation location = QStandardPaths::AppDataLocation)
{
    if (util::portable::IsPortable())
        return PathToQString(config::GetConfig().GetDefaultAppPath());
    return QStandardPaths::writableLocation(location);
}

/// Portable copy (portable.txt next to the exe): keep QSettings (window layout,
/// layouts, scenarios, recent files, ...) in data/settings instead of the
/// registry / plist. Call once at startup, before any QSettings is created.
inline void UsePortableSettingsIfPortable()
{
    if (!util::portable::IsPortable())
        return;
    const auto settingsDir = util::portable::DataDir() / "settings";
    std::error_code ec;
    std::filesystem::create_directories(settingsDir, ec);
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope, PathToQString(settingsDir));
}

/// The application's QSettings store. QSettings("LogViewer", "LogViewer")
/// always uses the native registry / plist and so ignores portable mode; this
/// follows QSettings::defaultFormat(). The explicit names keep using the same
/// native store as before outside portable mode (a default-constructed
/// QSettings would pick another plist on macOS, from the organisation domain).
class AppSettings : public QSettings
{
  public:
    AppSettings()
        : QSettings(QSettings::defaultFormat(), QSettings::UserScope,
                    QStringLiteral("LogViewer"), QStringLiteral("LogViewer"))
    {
    }
};

} // namespace ui::qt::utils
