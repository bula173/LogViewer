#pragma once

#include <string>
#include <vector>

/**
 * @namespace updates
 * @brief Data structures for the application and plugin update mechanism.
 *
 * These types are Qt-free so they can be used by both the core and UI layers.
 */
namespace updates {

/**
 * @brief Information about an available application update.
 */
struct AppUpdateInfo
{
    std::string version;      ///< Available version string, e.g. "1.2.0"
    std::string releaseUrl;   ///< GitHub release page URL
    std::string releaseNotes; ///< Markdown release notes from GitHub API
    bool        isNewer {false}; ///< True when availableVersion > currently running
};

/**
 * @brief Information about a single plugin that has an available update.
 *
 * The @c isCompatible flag is pre-computed by UpdateChecker based on the
 * plugin manifest's apiVersion and minAppVersion fields. Incompatible
 * updates are included in the result so the UI can display a warning, but
 * UpdateDialog must not allow the user to install them.
 */
struct PluginUpdateInfo
{
    std::string id;                ///< Plugin ID (matches PluginMetadata::id)
    std::string name;              ///< Human-readable name
    std::string currentVersion;    ///< Installed version ("" if not installed)
    std::string availableVersion;  ///< Version available in the release
    std::string downloadUrl;       ///< Platform-specific ZIP download URL
    std::string apiVersion;        ///< Plugin API version declared in manifest
    std::string minAppVersion;     ///< Minimum app version required by this plugin
    std::string sha256;            ///< Expected SHA-256 hex digest of the ZIP
    bool        isCompatible {false}; ///< API version and minAppVersion satisfied
};

/**
 * @brief Combined result of a single update check.
 *
 * Plugins vector contains only entries where availableVersion > currentVersion.
 * When an app update is not found, @c app.isNewer will be false.
 */
struct UpdateCheckResult
{
    AppUpdateInfo                 app;
    std::vector<PluginUpdateInfo> plugins;

    bool HasAnyUpdate() const
    {
        if (app.isNewer) return true;
        for (const auto& p : plugins)
            if (p.isCompatible) return true;
        return false;
    }
};

} // namespace updates
