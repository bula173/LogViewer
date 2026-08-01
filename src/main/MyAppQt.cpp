#include "Config.hpp"
#include "EventsContainer.hpp"
#include "Error.hpp"
#include "Version.hpp"
#include "MainController.hpp"
#include "qt/MainWindow.hpp"
#include "qt/StartupSplash.hpp"
#include "qt/utils/ThemeSwitcher.hpp"
#include "Logger.hpp"

#include <QApplication>
#include <QDir>
#include <QStandardPaths>
#include <QLibraryInfo>
#include <QPalette>
#include <QStyle>
#include <QStyleFactory>
#include <QFile>
#include <QFont>
#include <cstdlib>
#include <string>

#ifdef _WIN32
#include <windows.h>
#endif

namespace
{
constexpr const char* kQtAppName = "LogViewer";

void ApplyModernStyle(QApplication& app)
{
    ApplyTheme(app, 0);

    QFont appFont = app.font();
    appFont.setPointSize(10);
    appFont.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(appFont);
}

} // namespace

int main(int argc, char** argv)
{
    try
    {
        // QApplication must exist before any Qt widgets (including the splash).
        QApplication app(argc, argv);

        app.setApplicationName(kQtAppName);
        app.setApplicationVersion(
            QString::fromStdString(Version::current().asShortStr()));
        app.setOrganizationName("LogViewer");
        app.setOrganizationDomain("logviewer.app");

        ApplyModernStyle(app);

        // ── Splash screen ─────────────────────────────────────────────────
        ui::qt::StartupSplash splash(app.applicationVersion());
        splash.show();

        // ── Logging ───────────────────────────────────────────────────────
        splash.Step(QObject::tr("Initializing logging…"));
        try
        {
            auto& cfg = config::GetConfig();
            cfg.SetAppName(kQtAppName);
            util::Logger::Initialize(
                util::Logger::fromStrLevel(cfg.logLevel), cfg.GetAppLogPath());
            util::Logger::Info("Logging initialized for Qt UI");
        }
        catch (const std::exception& e)
        {
            splash.Warn(QObject::tr("Logging setup warning: %1")
                            .arg(QString::fromUtf8(e.what())));
        }

        // ── Configuration ─────────────────────────────────────────────────
        splash.Step(QObject::tr("Loading configuration…"));
        try
        {
            auto& cfg = config::GetConfig();
            cfg.SetAppName(kQtAppName);
            cfg.LoadConfig();

            // Apply log level from config (was initialized with default before LoadConfig)
            util::Logger::SetLevel(util::Logger::fromStrLevel(cfg.logLevel));
            util::Logger::Info("Configuration loaded successfully");
            util::Logger::Info("Current log level: {} (to change: Tools → Edit Config → logLevel)",
                               cfg.logLevel);
            util::Logger::Debug("Debug logging is enabled. View logs via Tools → Open App Log");
            util::Logger::Trace("Trace logging is enabled (maximum verbosity)");
        }
        catch (const std::exception& e)
        {
            const QString msg = QObject::tr("Configuration error: %1")
                                    .arg(QString::fromUtf8(e.what()));
            splash.Error(msg);
            util::Logger::Error("Configuration load failed: {}", e.what());
        }

        // ── Main window ───────────────────────────────────────────────────
        splash.Step(QObject::tr("Building user interface…"));

        db::EventsContainer events;
        mvc::MainController controller(events);

        ui::qt::MainWindow window(controller, events, &splash);

        window.show();
        splash.Finish(&window); // blocks here if any errors were reported

        util::Logger::Info("Main window shown successfully");
        return app.exec();
    }
    catch (const error::Error& ex)
    {
        util::Logger::Error("Fatal error: {}", ex.what());
        return EXIT_FAILURE;
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("Fatal Qt error: {}", ex.what());
        return EXIT_FAILURE;
    }
    catch (...)
    {
        util::Logger::Error("Unknown fatal error during startup");
        return EXIT_FAILURE;
    }
}
