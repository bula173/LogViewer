#include "config/Config.hpp"
#include "db/EventsContainer.hpp"
#include "error/Error.hpp"
#include "mvc/MainController.hpp"
#include "ui/qt/MainWindow.hpp"
#include "util/Logger.hpp"

#include <QApplication>
#include <QMessageBox>
#include <QDir>
#include <QStandardPaths>
#include <cstdlib>
#include <string>

namespace
{
constexpr const char* kQtAppName = "LogViewerQt";

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
    QMessageBox::critical(nullptr, "LogViewer Qt", text);
}

bool CheckQtLibraries()
{
    // Check if QtCore library is available
    // This helps catch DLL loading issues early
    try {
        // Try to access Qt version info
        QString qtVersion = qVersion();
        util::Logger::Info("Qt version: {}", qtVersion.toStdString());

        // Check application directory for Qt libraries
        QString appDir = QCoreApplication::applicationDirPath();
        util::Logger::Info("Application directory: {}", appDir.toStdString());

        // On macOS, Qt libraries are in frameworks
        #ifdef Q_OS_MAC
        QDir frameworksDir(appDir + "/../Frameworks");
        if (!frameworksDir.exists()) {
            util::Logger::Warn("Frameworks directory not found: {}", frameworksDir.absolutePath().toStdString());
        } else {
            QStringList frameworks = frameworksDir.entryList(QStringList() << "QtCore.framework", QDir::Dirs);
            if (frameworks.isEmpty()) {
                util::Logger::Warn("QtCore.framework not found in Frameworks directory");
            } else {
                util::Logger::Info("QtCore.framework found");
            }
        }
        #endif

        // Check Qt plugins path
        QStringList pluginPaths = QCoreApplication::libraryPaths();
        util::Logger::Info("Qt library paths:");
        for (const QString& path : pluginPaths) {
            util::Logger::Info("  {}", path.toStdString());
            QDir pluginDir(path);
            if (pluginDir.exists()) {
                QStringList plugins = pluginDir.entryList(QStringList() << "*qt*", QDir::Files);
                if (!plugins.isEmpty()) {
                    util::Logger::Info("    Found Qt plugins: {}", plugins.join(", ").toStdString());
                }
            }
        }

        return true;
    }
    catch (const std::exception& ex) {
        util::Logger::Error("Qt library check failed: {}", ex.what());
        return false;
    }
    catch (...) {
        util::Logger::Error("Qt library check failed with unknown error");
        return false;
    }
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
        // Check Qt libraries before creating QApplication
        if (!CheckQtLibraries()) {
            ShowFatalMessage("Qt library check failed. Please ensure Qt is properly installed and the Qt bin directory is in your PATH.");
            return EXIT_FAILURE;
        }

        // Create QApplication on the stack for proper cleanup
        QApplication app(argc, argv);
        util::Logger::Info("QApplication created successfully");

        db::EventsContainer events;
        mvc::MainController controller(events);

        ui::qt::MainWindow window(controller, events);
        window.show();

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
