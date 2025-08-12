// internal
#include "MyApp.hpp"
#include "config/Config.hpp"
#include "error/Error.hpp"
#include "gui/MainWindow.hpp"
#include "main/version.h"
// std
#include <filesystem>

wxIMPLEMENT_APP_NO_MAIN(MyApp);

int main(int argc, char** argv)
{
    try
    {
        if (!wxEntryStart(argc, argv))
            return 1;

        int code = 1;
        if (wxTheApp && wxTheApp->CallOnInit())
        {
            code = wxTheApp->OnRun();
            wxTheApp->OnExit();
        }

        wxEntryCleanup();
        return code;
    }
    catch (...)
    {
        spdlog::error("Unhandled exception occurred");
        wxMessageBox("An unexpected error occurred. Please check the logs.",
            "Error", wxOK | wxICON_ERROR);
        abort();
    }
}

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
    catch (const wxString& e)
    {
        spdlog::error("Failed to show main window: {}", e.ToStdString());
        wxMessageBox(wxString::Format("Failed to show main window:\n%s", e),
            "Error", wxOK | wxICON_ERROR);
        return false;
    }
    catch (const error::Error& e)
    {
        spdlog::error("Fatal application error: {}", e.what());
        return false;
    }
    catch (const std::exception& e)
    {
        spdlog::error("Fatal std::exception: {}", e.what());
        wxMessageBox(wxString::Format("Unhandled exception:\n%s", e.what()),
            "Error", wxOK | wxICON_ERROR);
        return false;
    }
    catch (...)
    {
        spdlog::error("Fatal unknown exception");
        wxMessageBox(
            "Unhandled unknown exception.", "Error", wxOK | wxICON_ERROR);
        return false;
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
    catch (const std::exception& ex)
    {
        spdlog::error("Exception while creating file sink: {}", ex.what());
    }
    catch (...)
    {
        spdlog::error("Unknown exception while creating file sink");
    }

    // Console sink for debug output

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