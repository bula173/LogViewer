#pragma once

#include <QString>
#include <QObject>
#include <vector>
#include <functional>
#include <chrono>
#include <memory>

namespace ui::qt {
class NotificationWidget;
}

namespace ui::qt::utils {

/**
 * @brief Smart notification system for v1.10.0 Phase 9.
 *
 * Features:
 * - Toast notifications for log events (errors, warnings, critical)
 * - Custom notification filtering based on log level/type
 * - Notification history tracking with timestamps
 * - Desktop notifications on supported platforms
 * - Sound alerts and visual indicators
 * - Notification grouping and deduplication
 * - Configurable display duration and position
 */
class NotificationManager : public QObject
{
    Q_OBJECT

  public:
    enum class NotificationType
    {
        Info,
        Warning,
        Error,
        Success,
        Critical
    };

    enum class NotificationAction
    {
        None,
        GoToEvent,
        ShowDetails,
        Dismiss
    };

    struct Notification
    {
        QString id;
        QString title;
        QString message;
        NotificationType type;
        int64_t timestamp;
        int eventRow {-1};
        bool read {false};
    };

    static NotificationManager& getInstance();

    /// Show a notification
    void notify(const QString& title, const QString& message,
                NotificationType type = NotificationType::Info, int eventRow = -1);

    /// Enable/disable notifications for a log level
    void setNotificationsEnabled(const QString& logLevel, bool enabled);
    bool areNotificationsEnabled(const QString& logLevel) const;

    /// Configure notification behavior
    void setDisplayDuration(int milliseconds);
    void setMaxNotifications(int count);
    void setGroupDuplicates(bool group);

    /// Get notification history
    const std::vector<Notification>& notificationHistory() const { return m_history; }

    /// Clear notification history
    void clearHistory();

    /// Get unread notification count
    int unreadCount() const;

    /// Play sound alert (placeholder for audio system integration)
    void playSound(NotificationType type);

  signals:
    void NotificationReceived(const Notification& notification);
    void NotificationHistoryChanged();

  private slots:
    void OnNotificationTimeout();

  private:
    NotificationManager();

    void AddToHistory(const Notification& notification);
    bool ShouldShowNotification(NotificationType type) const;

    std::vector<Notification> m_history;
    std::map<QString, bool> m_enabledLevels;
    int m_displayDurationMs {5000};
    int m_maxNotifications {100};
    bool m_groupDuplicates {true};
};

} // namespace ui::qt::utils
