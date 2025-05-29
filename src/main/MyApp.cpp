// internal
#include "MyApp.hpp"
#include "config/Config.hpp"
#include "gui/MainWindow.hpp"
#include "main/version.h"
// std
#include <filesystem>

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{

    if (!wxApp::OnInit())
        return false;

    setupLogging();

    spdlog::info("Initializing MyApp");

    setupConfig();
    ChangeLogLevel();

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
    std::vector<spdlog::sink_ptr> sinks;


    // Create both file and console sinks
    try
    {
        auto file_sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(
            config.GetAppLogPath(), true);
        file_sink->set_level(spdlog::level::from_str(config.logLevel));
        sinks.push_back(file_sink);
    }
    catch (const spdlog::spdlog_ex& ex)
    {
        spdlog::error("Failed to create file sink: {}", ex.what());
    }

    auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
    sinks.push_back(console_sink);

    spdlog::info("Log file path: {}", config.GetAppLogPath());

    console_sink->set_level(spdlog::level::from_str(config.logLevel));
    spdlog::info("Logging configuration loaded from config file. Log level: {}",
        config.logLevel);

    auto logger = std::make_shared<spdlog::logger>(
        "multi_sink", sinks.begin(), sinks.end());
    spdlog::set_default_logger(logger);
    spdlog::flush_on(spdlog::level::debug);

    spdlog::info("Setting up logging configuration");
}

void MyApp::ChangeLogLevel()
{
    auto& config = config::GetConfig();
    auto level = spdlog::level::from_str(config.logLevel);

    if (level != spdlog::get_level())
    {
        spdlog::set_level(level);
        spdlog::info("Log level changed to: {}", config.logLevel);
    }
}
// This is the main application class. It initializes the wxWidgets library