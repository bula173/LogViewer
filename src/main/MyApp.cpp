// internal
#include "MyApp.hpp"
#include "config/Config.hpp"
#include "gui/MainWindow.hpp"
#include "main/version.h"
// third party
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>
// std
#include <filesystem>

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{

    if (!wxApp::OnInit())
        return false;
    spdlog::info("Initializing MyApp");

    setupConfig();
    setupLogging();

    const auto& version = Version::current();

    gui::MainWindow* frame =
        new gui::MainWindow(m_appName + " " + version.asShortStr(),
            wxPoint(50, 50), wxSize(1000, 600), version);
    // frame->SetIcon(wxICON(app_icon)); // Set the application icon
    try
    {
        frame->Show(true);
    }
    catch (const std::exception& e)
    {
        std::cerr << e.what() << '\n';
    }


    spdlog::info("Main window created and shown");
    return true;
}

void MyApp::setupConfig()
{
    spdlog::info("Setting up configuration");

    // Set the log level based on build type
#ifdef NDEBUG
    spdlog::info("Release build");
#else
    spdlog::info("Debug build");
#endif

    spdlog::info(
        "Current working dir: {}", std::filesystem::current_path().string());

    auto& config = config::GetConfig();

    config.SetAppName(m_appName);
    spdlog::info("Application name set to: {}", config.appName);
    config.LoadConfig();
}

void MyApp::setupLogging()
{
    auto& config = config::GetConfig();
    // Create both file and console sinks
    auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
        config.GetAppLogPath(), true);
    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    spdlog::info("Log file path: {}", config.GetAppLogPath());


#ifdef NDEBUG
    auto& config = config::GetConfig();
    file_sink->set_level(spdlog::level::from_str(config.logLevel));
    console_sink->set_level(spdlog::level::from_str(config.logLevel));
    spdlog::info("Logging configuration loaded from config file. Log level: {}",
        config.logLevel);
#else
    file_sink->set_level(spdlog::level::debug);
    console_sink->set_level(spdlog::level::debug);
#endif

    std::vector<spdlog::sink_ptr> sinks {file_sink, console_sink};
    auto logger = std::make_shared<spdlog::logger>(
        "multi_sink", sinks.begin(), sinks.end());
    spdlog::set_default_logger(logger);

    spdlog::info("Setting up logging configuration");
}
// This is the main application class. It initializes the wxWidgets library