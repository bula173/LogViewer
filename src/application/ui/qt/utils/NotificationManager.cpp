#include "NotificationManager.hpp"

#include "Logger.hpp"

#include <QDateTime>
#include <QTimer>
#include <algorithm>

namespace ui::qt::utils {

NotificationManager& NotificationManager::getInstance()
{
    static NotificationManager instance;
    return instance;
}

NotificationManager::NotificationManager()
{
    // Initialize enabled levels - notifications for Error and Critical by default
    m_enabledLevels["DEBUG"] = false;
    m_enabledLevels["INFO"] = true;
    m_enabledLevels["WARNING"] = true;
    m_enabledLevels["ERROR"] = true;
    m_enabledLevels["CRITICAL"] = true;
}

void NotificationManager::notify(const QString& title, const QString& message,
                                 NotificationType type, int eventRow)
{
    if (!ShouldShowNotification(type))
        return;

    Notification notif;
    notif.id = QString::number(QDateTime::currentMSecsSinceEpoch());
    notif.title = title;
    notif.message = message;
    notif.type = type;
    notif.timestamp = QDateTime::currentMSecsSinceEpoch();
    notif.eventRow = eventRow;
    notif.read = false;

    // Check for duplicates if grouping enabled
    if (m_groupDuplicates)
    {
        auto it = std::find_if(m_history.rbegin(), m_history.rend(),
            [&notif](const Notification& n) {
                return n.title == notif.title && n.message == notif.message &&
                       (notif.timestamp - n.timestamp) < 1000;  // Within 1 second
            });
        if (it != m_history.rend())
        {
            util::Logger::Debug("[NotificationManager] Duplicate notification grouped");
            return;
        }
    }

    AddToHistory(notif);
    playSound(type);

    util::Logger::Info("[NotificationManager] Notification: {} - {}", title.toStdString(), message.toStdString());
    emit NotificationReceived(notif);
}

void NotificationManager::setNotificationsEnabled(const QString& logLevel, bool enabled)
{
    m_enabledLevels[logLevel] = enabled;
    util::Logger::Debug("[NotificationManager] Notifications for {} {}", logLevel.toStdString(),
                       enabled ? "enabled" : "disabled");
}

bool NotificationManager::areNotificationsEnabled(const QString& logLevel) const
{
    auto it = m_enabledLevels.find(logLevel);
    return (it != m_enabledLevels.end()) ? it->second : false;
}

void NotificationManager::setDisplayDuration(int milliseconds)
{
    m_displayDurationMs = milliseconds;
    util::Logger::Debug("[NotificationManager] Display duration set to {}ms", milliseconds);
}

void NotificationManager::setMaxNotifications(int count)
{
    m_maxNotifications = count;
    util::Logger::Debug("[NotificationManager] Max notifications set to {}", count);
}

void NotificationManager::setGroupDuplicates(bool group)
{
    m_groupDuplicates = group;
    util::Logger::Debug("[NotificationManager] Duplicate grouping {}", group ? "enabled" : "disabled");
}

void NotificationManager::clearHistory()
{
    m_history.clear();
    emit NotificationHistoryChanged();
    util::Logger::Info("[NotificationManager] History cleared");
}

int NotificationManager::getUnreadCount() const
{
    return std::count_if(m_history.begin(), m_history.end(),
        [](const Notification& n) { return !n.read; });
}

void NotificationManager::playSound(NotificationType type)
{
    // Map notification types to sound files
    QString soundFile;
    switch (type)
    {
        case NotificationType::Info:
            // Subtle beep for info
            break;
        case NotificationType::Warning:
            // Medium beep for warning
            break;
        case NotificationType::Error:
            // Alert sound for error
            break;
        case NotificationType::Success:
            // Positive sound for success
            break;
        case NotificationType::Critical:
            // Loud alert for critical
            break;
    }

    if (!soundFile.isEmpty())
    {
        util::Logger::Debug("[NotificationManager] Playing sound: {}", soundFile.toStdString());
        // QSoundEffect would be used here on platforms with sound support
    }
}

void NotificationManager::OnNotificationTimeout()
{
    // Notification widget will auto-hide after timeout
}

void NotificationManager::AddToHistory(const Notification& notification)
{
    m_history.push_back(notification);

    // Remove oldest if exceeding max
    if (static_cast<int>(m_history.size()) > m_maxNotifications)
    {
        m_history.erase(m_history.begin());
    }

    emit NotificationHistoryChanged();
}

bool NotificationManager::ShouldShowNotification(NotificationType type) const
{
    // Map type to log level
    QString logLevel;
    switch (type)
    {
        case NotificationType::Info: logLevel = "INFO"; break;
        case NotificationType::Warning: logLevel = "WARNING"; break;
        case NotificationType::Error: logLevel = "ERROR"; break;
        case NotificationType::Success: logLevel = "INFO"; break;
        case NotificationType::Critical: logLevel = "CRITICAL"; break;
    }

    auto it = m_enabledLevels.find(logLevel);
    return (it != m_enabledLevels.end()) ? it->second : true;
}

} // namespace ui::qt::utils
