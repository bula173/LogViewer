#pragma once

#include "Config.hpp"
#include "PortableMode.hpp"

#include <QStandardPaths>
#include <QString>

namespace ui::qt::utils
{

/// Per-user data directory: the `data/` folder next to the executable in
/// portable mode, otherwise Qt's standard location @p location.
inline QString AppDataDir(QStandardPaths::StandardLocation location = QStandardPaths::AppDataLocation)
{
    if (util::portable::IsPortable())
        return QString::fromStdString(config::GetConfig().GetDefaultAppPath().string());
    return QStandardPaths::writableLocation(location);
}

} // namespace ui::qt::utils
