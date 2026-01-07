#include "Config.hpp"
#include "EventsContainer.hpp"
#include "Error.hpp"
#include "Version.hpp"
#include "MainController.hpp"
#include "qt/MainWindow.hpp"
#include "qt/ThemeSwitcher.hpp"
#include "Logger.hpp"

#include <QApplication>
#include <QMessageBox>
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
    // Apply dark theme by default
    ApplyTheme(app, 0);
    
    // Set application font for better typography
    // Use system default font with better sizing
    QFont appFont = app.font();
    appFont.setPointSize(10);
    appFont.setStyleStrategy(QFont::PreferAntialias);
    app.setFont(appFont);
}

bool SetupConfig()
{
    auto& config = config::GetConfig();
    config.SetAppName(kQtAppName);
    config.LoadConfig();
    return true;
}

bool SetupLogging()
{
    auto& config = config::GetConfig();
    config.SetAppName(kQtAppName);
    util::Logger::Initialize(
        util::Logger::fromStrLevel(config.logLevel), config.GetAppLogPath());
    util::Logger::Info("Logging initialized for Qt UI");
    util::Logger::Info(
            "Logging configuration loaded from config file. Log level: {}",
            config.logLevel);
        util::Logger::Info("Log file path: {}", config.GetAppLogPath());
    return true;
}

void ShowFatalMessage(const QString& text)
{
    util::Logger::Error("Fatal error: {}", text.toStdString());
}

} // namespace

int main(int argc, char** argv)
{
    

    try
    {
        SetupLogging();
        util::Logger::Info("Starting LogViewer Qt application");
        util::Logger::Info("Initializing configuration");
        util::Logger::Info("PrintConfig: ");
        config::GetConfig().GetPrintConfig();
        SetupConfig();

    }
    catch (const error::Error& ex)
    {
        ShowFatalMessage(QString::fromStdString(ex.what()));
        return EXIT_FAILURE;
    }
    catch (const std::exception& ex)
    {
        ShowFatalMessage(QStringLiteral("Qt initialization failed: ") +
            QString::fromUtf8(ex.what()));
        return EXIT_FAILURE;
    }

    try
    {
        // Create QApplication on the stack for proper cleanup
        QApplication app(argc, argv);
        util::Logger::Info("QApplication created successfully");

        // Set application properties
        app.setApplicationName(kQtAppName);
        app.setApplicationVersion(QString::fromStdString(Version::current().asShortStr()));
        app.setOrganizationName("LogViewer");
        app.setOrganizationDomain("logviewer.app");
        
        // Apply modern styling
        ApplyModernStyle(app);

        // Verify QApplication is properly initialized
        if (!QApplication::instance()) {
            ShowFatalMessage("Failed to create QApplication instance");
            return EXIT_FAILURE;
        }

        util::Logger::Info("Qt application properties set successfully");

        db::EventsContainer events;
        mvc::MainController controller(events);

        ui::qt::MainWindow window(controller, events);
        window.show();

        util::Logger::Info("Main window shown successfully");

        return app.exec();
    } catch (const error::Error& ex)
    {
        ShowFatalMessage(QString::fromStdString(ex.what()));
        return EXIT_FAILURE;
    }
    catch (const std::exception& ex)
    {
        ShowFatalMessage(QStringLiteral("Qt runtime error: ") +
            QString::fromUtf8(ex.what()));
        return EXIT_FAILURE;
    } catch (...)
    {
        ShowFatalMessage(QStringLiteral("An unknown fatal error occurred. This may be due to missing Qt libraries."));
        return EXIT_FAILURE;
    }

}
