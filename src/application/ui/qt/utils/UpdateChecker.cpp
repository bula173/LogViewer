#include "UpdateChecker.hpp"

#include "Logger.hpp"

#include <cstdio>
#include <sstream>
#include <string>
#include "PluginManager.hpp"
#include "Version.hpp"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkRequest>
#include <QStandardPaths>

#ifdef _WIN32
  #define LOGVIEWER_PLATFORM "windows"
#elif defined(__APPLE__)
  #define LOGVIEWER_PLATFORM "macos"
#else
  #define LOGVIEWER_PLATFORM "linux"
#endif

namespace ui::qt {

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

QString UpdateChecker::PlatformId()
{
    return QString::fromLatin1(LOGVIEWER_PLATFORM);
}

std::string UpdateChecker::StripV(const QString& tagName)
{
    QString s = tagName.trimmed();
    if (s.startsWith('v') || s.startsWith('V'))
        s = s.mid(1);
    return s.toStdString();
}

bool UpdateChecker::VersionGreater(const std::string& a, const std::string& b)
{
    // Parse "major.minor.patch" into ints for comparison.
    auto parse = [](const std::string& s) -> std::array<int, 3> {
        std::array<int, 3> v {0, 0, 0};
        std::istringstream ss(s);
        char dot;
        ss >> v[0] >> dot >> v[1] >> dot >> v[2];
        return v;
    };
    const auto va = parse(a);
    const auto vb = parse(b);
    return va > vb;
}

bool UpdateChecker::IsPluginCompatible(const updates::PluginUpdateInfo& info)
{
    // API major must match; minor must be in [0, 1] (same as PluginManager's
    // kSupportedApiMajor / kSupportedApiMaxMinor constraints).
    const std::string& api = info.apiVersion;
    int apimaj = 0, apimin = 0;
    std::sscanf(api.c_str(), "%d.%d", &apimaj, &apimin);
    if (apimaj != 1 || apimin < 0 || apimin > 1)
        return false;

    // Check minAppVersion if specified.
    if (!info.minAppVersion.empty())
    {
        const auto& cur = Version::current();
        std::string curStr = std::to_string(cur.major) + '.' +
                             std::to_string(cur.minor) + '.' +
                             std::to_string(cur.patch);
        // Current app must be >= minAppVersion.
        if (VersionGreater(info.minAppVersion, curStr))
            return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Construction
// ---------------------------------------------------------------------------

UpdateChecker::UpdateChecker(QObject* parent)
    : QObject(parent)
{
}

// ---------------------------------------------------------------------------
// CheckAsync — step 1: hit the releases/latest endpoint
// ---------------------------------------------------------------------------

void UpdateChecker::CheckAsync()
{
    if (m_checking)
    {
        util::Logger::Debug("[UpdateChecker] Already checking — ignoring duplicate call");
        return;
    }

    m_checking = true;
    m_pending  = {};

    const auto& ver = Version::current();
    const QString ua = QStringLiteral("LogViewer/%1")
                           .arg(QString::fromStdString(ver.asShortStr()));

    QNetworkRequest req(QUrl(QString::fromLatin1(kGitHubApiUrl)));
    req.setRawHeader("User-Agent", ua.toUtf8());
    req.setRawHeader("Accept", "application/vnd.github+json");
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = m_net.get(req);
    connect(reply, &QNetworkReply::finished, this,
            [this, reply]() { OnReleaseReply(reply); });

    util::Logger::Info("[UpdateChecker] Checking for updates at {}", kGitHubApiUrl);
}

// ---------------------------------------------------------------------------
// Step 1 reply handler — parse GitHub release JSON
// ---------------------------------------------------------------------------

void UpdateChecker::OnReleaseReply(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError)
    {
        m_checking = false;
        const QString err = reply->errorString();
        util::Logger::Warn("[UpdateChecker] Network error: {}", err.toStdString());
        emit UpdateCheckFailed(err);
        return;
    }

    const QByteArray data = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
    {
        m_checking = false;
        const QString err = tr("Invalid JSON from GitHub API");
        util::Logger::Warn("[UpdateChecker] {}", err.toStdString());
        emit UpdateCheckFailed(err);
        return;
    }

    const QJsonObject root = doc.object();
    const QString tagName  = root.value(QStringLiteral("tag_name")).toString();
    const QString htmlUrl  = root.value(QStringLiteral("html_url")).toString();
    const QString body     = root.value(QStringLiteral("body")).toString();

    const std::string available = StripV(tagName);
    const auto& cur = Version::current();
    const std::string current = std::to_string(cur.major) + '.' +
                                std::to_string(cur.minor) + '.' +
                                std::to_string(cur.patch);

    m_pending.app.version      = available;
    m_pending.app.releaseUrl   = htmlUrl.toStdString();
    m_pending.app.releaseNotes = body.toStdString();
    m_pending.app.isNewer      = VersionGreater(available, current);

    util::Logger::Info("[UpdateChecker] Latest release: {} (current: {}, isNewer: {})",
                       available, current, m_pending.app.isNewer);

    // Look for plugins_manifest.json asset in this release.
    const QJsonArray assets = root.value(QStringLiteral("assets")).toArray();
    QString manifestUrl;
    for (const auto& assetVal : assets)
    {
        const QJsonObject asset = assetVal.toObject();
        if (asset.value(QStringLiteral("name")).toString() == QStringLiteral("plugins_manifest.json"))
        {
            manifestUrl = asset.value(QStringLiteral("browser_download_url")).toString();
            break;
        }
    }

    if (manifestUrl.isEmpty())
    {
        util::Logger::Info("[UpdateChecker] No plugins_manifest.json in release assets");
        m_checking = false;
        emit UpdateCheckComplete(m_pending);
        return;
    }

    // Step 2: fetch the manifest.
    QUrl manifestQUrl(manifestUrl);
    QNetworkRequest req{manifestQUrl};
    req.setRawHeader("User-Agent",
                     QStringLiteral("LogViewer/%1")
                         .arg(QString::fromStdString(cur.asShortStr()))
                         .toUtf8());
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* manifestReply = m_net.get(req);
    connect(manifestReply, &QNetworkReply::finished, this,
            [this, manifestReply]() { OnManifestReply(manifestReply); });
}

// ---------------------------------------------------------------------------
// Step 2 reply handler — parse plugins_manifest.json
// ---------------------------------------------------------------------------

void UpdateChecker::OnManifestReply(QNetworkReply* reply)
{
    reply->deleteLater();

    if (reply->error() != QNetworkReply::NoError)
    {
        util::Logger::Warn("[UpdateChecker] Failed to fetch plugin manifest: {}",
                           reply->errorString().toStdString());
        // Still emit the app-only result.
        m_checking = false;
        emit UpdateCheckComplete(m_pending);
        return;
    }

    const QByteArray data = reply->readAll();
    const QJsonDocument doc = QJsonDocument::fromJson(data);
    if (!doc.isObject())
    {
        util::Logger::Warn("[UpdateChecker] Plugin manifest is not valid JSON");
        m_checking = false;
        emit UpdateCheckComplete(m_pending);
        return;
    }

    const QJsonArray plugins = doc.object().value(QStringLiteral("plugins")).toArray();
    const QString platform   = PlatformId();
    auto& pm = plugin::PluginManager::GetInstance();

    for (const auto& val : plugins)
    {
        const QJsonObject p = val.toObject();
        const QString id    = p.value(QStringLiteral("id")).toString();

        updates::PluginUpdateInfo info;
        info.id               = id.toStdString();
        info.name             = p.value(QStringLiteral("name")).toString().toStdString();
        info.availableVersion = p.value(QStringLiteral("version")).toString().toStdString();
        info.apiVersion       = p.value(QStringLiteral("apiVersion")).toString().toStdString();
        info.minAppVersion    = p.value(QStringLiteral("minAppVersion")).toString().toStdString();

        // Installed version (empty = not installed; still offerable as fresh install)
        auto* installed = pm.GetPlugin(info.id);
        if (installed)
            info.currentVersion = installed->GetMetadata().version;

        // Platform-specific download URL and sha256
        const QJsonObject assets = p.value(QStringLiteral("assets")).toObject();
        info.downloadUrl = assets.value(platform).toString().toStdString();

        const QJsonObject hashes = p.value(QStringLiteral("sha256")).toObject();
        info.sha256 = hashes.value(platform).toString().toStdString();

        if (info.downloadUrl.empty())
        {
            util::Logger::Debug("[UpdateChecker] No {} asset for plugin {}, skipping",
                                platform.toStdString(), info.id);
            continue;
        }

        info.isCompatible = IsPluginCompatible(info);

        // Only include if there is actually a newer version available.
        const bool hasUpdate = info.currentVersion.empty()
                               || VersionGreater(info.availableVersion, info.currentVersion);
        if (hasUpdate)
        {
            util::Logger::Info("[UpdateChecker] Plugin {} update: {} → {} (compat={})",
                               info.id, info.currentVersion, info.availableVersion,
                               info.isCompatible);
            m_pending.plugins.push_back(std::move(info));
        }
    }

    m_checking = false;
    emit UpdateCheckComplete(m_pending);
}

// ---------------------------------------------------------------------------
// DownloadPlugin
// ---------------------------------------------------------------------------

void UpdateChecker::DownloadPlugin(const updates::PluginUpdateInfo& info)
{
    if (info.downloadUrl.empty())
    {
        emit PluginDownloadFailed(QString::fromStdString(info.id),
                                  tr("No download URL for this platform"));
        return;
    }

    const QString pluginId = QString::fromStdString(info.id);

    QNetworkRequest req(QUrl(QString::fromStdString(info.downloadUrl)));
    req.setRawHeader("User-Agent",
                     QStringLiteral("LogViewer/%1")
                         .arg(QString::fromStdString(Version::current().asShortStr()))
                         .toUtf8());
    req.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                     QNetworkRequest::NoLessSafeRedirectPolicy);

    QNetworkReply* reply = m_net.get(req);

    connect(reply, &QNetworkReply::downloadProgress, this,
            [this, pluginId](qint64 recv, qint64 total) {
                emit PluginDownloadProgress(pluginId, recv, total);
            });

    connect(reply, &QNetworkReply::finished, this,
            [this, reply, info, pluginId]() {
                DoPluginDownload(info, reply);
                reply->deleteLater();
            });

    util::Logger::Info("[UpdateChecker] Downloading plugin {}: {}", info.id, info.downloadUrl);
}

void UpdateChecker::DoPluginDownload(const updates::PluginUpdateInfo& info,
                                     QNetworkReply* reply)
{
    const QString pluginId = QString::fromStdString(info.id);

    if (reply->error() != QNetworkReply::NoError)
    {
        const QString err = reply->errorString();
        util::Logger::Error("[UpdateChecker] Plugin download failed {}: {}", info.id,
                            err.toStdString());
        emit PluginDownloadFailed(pluginId, err);
        return;
    }

    const QByteArray data = reply->readAll();

    // Verify SHA-256 if provided
    if (!info.sha256.empty())
    {
        const QString computed =
            QString::fromLatin1(
                QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex());
        const QString expected = QString::fromStdString(info.sha256).toLower();
        if (computed.toLower() != expected)
        {
            const QString err =
                tr("Checksum mismatch for plugin %1").arg(pluginId);
            util::Logger::Error("[UpdateChecker] {}", err.toStdString());
            emit PluginDownloadFailed(pluginId, err);
            return;
        }
    }

    // Write to the app-local data directory (not the system Temp folder).
    // Using TempLocation triggers Windows Defender's dropper heuristic:
    //   network download -> write binary to %TEMP% -> load/execute it.
    // AppLocalDataLocation is the app's own sandbox directory which AV
    // vendors explicitly recognise as a safe staging area for self-updates.
    const QString downloadDir =
        QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) +
        QStringLiteral("/plugin_downloads");
    QDir().mkpath(downloadDir);

    const QString tempPath = downloadDir +
        QStringLiteral("/LogViewer_plugin_%1.zip").arg(pluginId);

    QFile f(tempPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
        const QString err =
            tr("Cannot write temp file: %1").arg(f.errorString());
        util::Logger::Error("[UpdateChecker] {}", err.toStdString());
        emit PluginDownloadFailed(pluginId, err);
        return;
    }
    f.write(data);
    f.close();

    util::Logger::Info("[UpdateChecker] Plugin {} downloaded to {}", info.id,
                       tempPath.toStdString());
    emit PluginDownloadComplete(pluginId, tempPath);
}

} // namespace ui::qt
