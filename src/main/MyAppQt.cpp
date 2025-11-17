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
    return true;
}

void ApplyConfiguredLogLevel()
{
    auto& config = config::GetConfig();
    auto level = util::Logger::fromStrLevel(config.logLevel);
    util::Logger::GetInstance()->setLevel(level);
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
        util::Logger::Info("Starting LogViewer Qt application");
        util::Logger::Info("Initializing configuration");
        SetupConfig();
        util::Logger::Info("PrintConfig: ");
        config::GetConfig().GetPrintConfig();
        util::Logger::Info("Initializing logging");
        SetupLogging();
        util::Logger::Info("Applying configured log level");
        ApplyConfiguredLogLevel();
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
