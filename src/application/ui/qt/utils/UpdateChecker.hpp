#pragma once

#include "UpdateInfo.hpp"

#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QObject>
#include <QString>

namespace ui::qt {

/**
 * @brief Asynchronous update checker using Qt6::Network.
 *
 * Queries the GitHub Releases API for bula173/LogViewer and optionally
 * downloads the plugins_manifest.json asset listed in that release to
 * enumerate available plugin updates.
 *
 * Usage:
 * @code
 *   auto* checker = new UpdateChecker(this);
 *   connect(checker, &UpdateChecker::UpdateCheckComplete, this, &MyClass::OnUpdateResult);
 *   checker->CheckAsync();
 * @endcode
 */
class UpdateChecker : public QObject
{
    Q_OBJECT

  public:
    explicit UpdateChecker(QObject* parent = nullptr);

    /**
     * @brief Start an asynchronous update check.
     *
     * Emits either UpdateCheckComplete or UpdateCheckFailed when finished.
     * Safe to call from the main thread.
     */
    void CheckAsync();

    /**
     * @brief Download a plugin ZIP to a temp location then verify its checksum.
     *
     * Emits PluginDownloadProgress, then PluginDownloadComplete or
     * PluginDownloadFailed when done.
     *
     * @param info   Plugin update info including downloadUrl and sha256.
     */
    void DownloadPlugin(const updates::PluginUpdateInfo& info);

    /** @brief True while an update check is in progress. */
    bool IsChecking() const { return m_checking; }

  signals:
    void UpdateCheckComplete(updates::UpdateCheckResult result);
    void UpdateCheckFailed(QString errorMessage);

    void PluginDownloadProgress(QString pluginId, qint64 bytesReceived,
                                qint64 bytesTotal);
    void PluginDownloadComplete(QString pluginId, QString tempFilePath);
    void PluginDownloadFailed(QString pluginId, QString errorMessage);

  private slots:
    void OnReleaseReply(QNetworkReply* reply);
    void OnManifestReply(QNetworkReply* reply);

  private:
    void DoPluginDownload(const updates::PluginUpdateInfo& info,
                          QNetworkReply* reply);

    /** Returns "linux", "windows", or "macos" based on the build target. */
    static QString PlatformId();

    /**
     * @brief Returns true when manifest apiVersion and minAppVersion are
     *        compatible with the running application.
     */
    static bool IsPluginCompatible(const updates::PluginUpdateInfo& info);

    /** Strip leading 'v' from a GitHub tag name ("v1.2.0" → "1.2.0"). */
    static std::string StripV(const QString& tagName);

    /**
     * @brief Semantic version comparison: returns true when a > b.
     * Handles "major.minor.patch" strings.
     */
    static bool VersionGreater(const std::string& a, const std::string& b);

    QNetworkAccessManager      m_net;
    updates::UpdateCheckResult m_pending;   ///< Assembled during the two-step check
    bool                       m_checking {false};

    static constexpr const char* kGitHubApiUrl =
        "https://api.github.com/repos/bula173/LogViewer/releases/latest";
};

} // namespace ui::qt
