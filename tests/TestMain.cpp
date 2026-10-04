#include <gtest/gtest.h>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <system_error>
#ifndef _WIN32
#include <unistd.h>
#else
#include <process.h>
#define getpid _getpid
#endif

#include "Config.hpp"
#include "qt/utils/AppDataDir.hpp"

#include <QSettings>

namespace
{
// Tests must never read or write the developer's real profile: view tests save
// column widths through Config::SaveConfig(), panels save actors.json, themes,
// keybindings, ... into the per-user app directory. Point every per-user path
// at a fresh temporary directory before any test runs.
void IsolateUserProfile()
{
    std::error_code ec;
    const auto home = std::filesystem::temp_directory_path(ec) /
        ("logviewer_tests_home_" + std::to_string(::getpid()));
    std::filesystem::remove_all(home, ec);
    std::filesystem::create_directories(home, ec);
    const std::string h = home.string();
#ifdef _WIN32
    _putenv_s("APPDATA", h.c_str());
    _putenv_s("LOCALAPPDATA", h.c_str());
    _putenv_s("USERPROFILE", h.c_str());
#else
    ::setenv("HOME", h.c_str(), 1);
    ::setenv("XDG_CONFIG_HOME", (h + "/.config").c_str(), 1);
    ::setenv("XDG_DATA_HOME", (h + "/.local/share").c_str(), 1);
#endif
    // The Config singleton resolved its paths during static initialisation,
    // before the environment above was changed.
    auto& cfg = config::GetConfig();
    const auto appDir = cfg.GetDefaultAppPath();
    cfg.SetConfigFilePath((appDir / "config.json").string());
    cfg.SetDictionaryFilePath((appDir / "field_dictionary.json").string());
    // QSettings (layouts, scenarios, window state): an INI file in the temp
    // profile instead of the real registry / plist. Code that persists
    // settings uses ui::qt::utils::AppSettings, which follows the default format.
    QSettings::setDefaultFormat(QSettings::IniFormat);
    QSettings::setPath(QSettings::IniFormat, QSettings::UserScope,
                       ui::qt::utils::PathToQString(appDir / "settings"));
}
} // namespace

// Custom main() (replaces gtest_main) so we can skip static/atexit teardown
// after tests finish. Several tests construct a real QApplication; on Linux
// CI with the "offscreen" QPA platform, Qt's static teardown segfaults after
// main() would otherwise return -- by which point RUN_ALL_TESTS() has already
// fully reported every test's outcome, so terminating immediately here loses
// nothing and avoids a false-negative CI failure.
int main(int argc, char** argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    IsolateUserProfile();
    const int result = RUN_ALL_TESTS();
    std::_Exit(result);
}
