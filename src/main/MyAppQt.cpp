#include "config/Config.hpp"
#include "db/EventsContainer.hpp"
#include "error/Error.hpp"
#include "mvc/MainController.hpp"
#include "ui/qt/MainWindow.hpp"
#include "util/Logger.hpp"

#include <QApplication>
#include <QMessageBox>
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
} // namespace

int main(int argc, char** argv)
{
    QApplication app(argc, argv);

    try
    {
        SetupLogging();
        util::Logger::Info("Starting LogViewer Qt application");
        util::Logger::Info("Initializing configuration");
        SetupConfig();
        util::Logger::Info("PrintConfig: ");
        config::GetConfig().GetPrintConfig();
        util::Logger::Info("Initializing logging");
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

    db::EventsContainer events;
    mvc::MainController controller(events);

    ui::qt::MainWindow window(controller, events);
    window.show();

    return app.exec();
}
