// internal
#include "MyApp.hpp"
#include "config/Config.hpp"
#include "error/Error.hpp"
#include "gui/MainWindow.hpp"
#include "main/version.h"
#include "util/Logger.hpp"
// std
#include <chrono>
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
        util::Logger::Error("Unhandled exception occurred");
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

    // Initialize locale for proper Unicode/Polish character support
    // This must be done before any GUI elements are created
    if (!m_locale.Init(wxLANGUAGE_DEFAULT, wxLOCALE_LOAD_DEFAULT))
    {
        util::Logger::Warn("Failed to initialize locale, using default");
    }
    
    util::Logger::Info("Initializing MyApp");

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
        util::Logger::Error("Failed to show main window: {}", e.ToStdString());
        wxMessageBox(wxString::Format("Failed to show main window:\n%s", e),
            "Error", wxOK | wxICON_ERROR);
        return false;
    }
    catch (const error::Error& e)
    {
        util::Logger::Error("Fatal application error: {}", e.what());
        return false;
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("Fatal std::exception: {}", e.what());
        wxMessageBox(wxString::Format("Unhandled exception:\n%s", e.what()),
            "Error", wxOK | wxICON_ERROR);
        return false;
    }
    catch (...)
    {
        util::Logger::Error("Fatal unknown exception");
        wxMessageBox(
            "Unhandled unknown exception.", "Error", wxOK | wxICON_ERROR);
        return false;
    }

    util::Logger::Info("Main window created and shown");
    return true;
}

void MyApp::setupConfig()
{
    util::Logger::Info("Setting up configuration");

    // Set the log level based on build type
#ifdef NDEBUG
    util::Logger::Info("Release build");
#else
    util::Logger::Info("Debug build");
    util::Logger::SetLevel(util::LogLevel::Debug);
#endif

    util::Logger::Info(
        "Current working dir: {}", std::filesystem::current_path().string());

    auto& config = config::GetConfig();

    config.SetAppName(m_appName);
    util::Logger::Info("Application name set to: {}", config.appName);
    config.LoadConfig();
}

void MyApp::setupLogging()
{
    auto& config = config::GetConfig();
    util::Logger::Initialize(util::Logger::fromStrLevel(config.logLevel), config.GetAppLogPath());

    util::Logger::Info("Setting up logging configuration");
    util::Logger::Info("Logging configuration loaded from config file. Log level: {}",
        config.logLevel);
    util::Logger::Info("Log file path: {}", config.GetAppLogPath());
}

void MyApp::ChangeLogLevel()
{
    auto& config = config::GetConfig();
    auto level = util::Logger::fromStrLevel(config.logLevel);

    if (level != util::Logger::GetInstance()->getLevel())
    {
        util::Logger::GetInstance()->setLevel(level);
        util::Logger::Info("Log level changed to: {}", config.logLevel);
    }
}
// This is the main application class. It initializes the wxWidgets library