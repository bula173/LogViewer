#include "MyApp.hpp"
#include "config/Config.hpp"
#include "error/Error.hpp"
#include "ui/wx/MainWindow.hpp"
#include "main/version.h"
#include "ui/UiServices.hpp"
#include "ui/wx/WxErrorPresenter.hpp"
#include "util/Logger.hpp"
// std
#include <cstdlib>
#include <filesystem>
#include <memory>

wxIMPLEMENT_APP_NO_MAIN(MyApp);

/**
 * @brief Application entry point bridging the wxWidgets lifecycle.
 */
int main(int argc, char** argv)
{
    try
    {
        if (!wxEntryStart(argc, argv))
            return EXIT_FAILURE;

        int code = EXIT_FAILURE;
        if (wxTheApp && wxTheApp->CallOnInit())
        {
            code = wxTheApp->OnRun();
            wxTheApp->OnExit();
        }

        wxEntryCleanup();
        return code;
    }
    catch (const error::Error& ex)
    {
        util::Logger::Critical("Unhandled application error: {}", ex.what());
        wxMessageBox(wxString::Format("Fatal application error:\n%s", ex.what()),
            "Error", wxOK | wxICON_ERROR);
    }
    catch (const std::exception& ex)
    {
        util::Logger::Critical("Unhandled std::exception: {}", ex.what());
        wxMessageBox(wxString::Format("Unhandled exception:\n%s", ex.what()),
            "Error", wxOK | wxICON_ERROR);
    }
    catch (...)
    {
        util::Logger::Critical("Unhandled unknown exception occurred");
        wxMessageBox("An unexpected error occurred. Please check the logs.",
            "Error", wxOK | wxICON_ERROR);
    }
    return EXIT_FAILURE;
}

bool MyApp::OnInit()
{
    if (!wxApp::OnInit())
        return false;

    ui::UiServices::Instance().SetErrorPresenter(
        std::make_shared<ui::wx::WxErrorPresenter>());

    const auto loggingResult = setupLogging();
    if (loggingResult.isErr())
    {
        util::Logger::Critical("Failed to initialize logging: {}",
            loggingResult.error().what());
        return false;
    }

    const auto localeResult = initializeLocale();
    if (localeResult.isErr())
    {
        util::Logger::Warn("{}", localeResult.error().what());
    }
    
    util::Logger::Info("Initializing MyApp");

    const auto configResult = setupConfig();
    if (configResult.isErr())
    {
        util::Logger::Error("Configuration initialization failed: {}",
            configResult.error().what());
        wxMessageBox(wxString::Format("Configuration error:\n%s",
                          configResult.error().what()),
            "Error", wxOK | wxICON_ERROR);
        return false;
    }
    ChangeLogLevel();

    const auto& version = Version::current();

    ui::wx::MainWindow* frame =
        new ui::wx::MainWindow(m_appName + " " + version.asShortStr(),
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

/**
 * @brief Load persisted configuration and ensure application metadata is set.
 */
MyApp::AppResult MyApp::setupConfig()
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

    try
    {
        config.SetAppName(m_appName);
        util::Logger::Info("Application name set to: {}", config.appName);
        config.LoadConfig();
    }
    catch (const error::Error& e)
    {
        util::Logger::Error("Failed to load config: {}", e.what());
        return AppResult::Err(error::Error(e.code(), e.what(), false));
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("Unexpected configuration failure: {}", e.what());
        return AppResult::Err(error::Error(error::ErrorCode::RuntimeError,
            std::string("Unexpected configuration failure: ") + e.what(), false));
    }

    return AppResult::Ok(std::monostate {});
}

/**
 * @brief Initialize the logger sinks based on configuration.
 */
MyApp::AppResult MyApp::setupLogging()
{
    try
    {
        auto& config = config::GetConfig();
        util::Logger::Initialize(
            util::Logger::fromStrLevel(config.logLevel), config.GetAppLogPath());

        util::Logger::Info("Setting up logging configuration");
        util::Logger::Info(
            "Logging configuration loaded from config file. Log level: {}",
            config.logLevel);
        util::Logger::Info("Log file path: {}", config.GetAppLogPath());
    }
    catch (const error::Error& e)
    {
        return AppResult::Err(error::Error(e.code(), e.what(), false));
    }
    catch (const std::exception& e)
    {
        return AppResult::Err(error::Error(error::ErrorCode::RuntimeError,
            std::string("Failed to set up logging: ") + e.what(), false));
    }

    return AppResult::Ok(std::monostate {});
}

/**
 * @brief Apply runtime log-level changes from the configuration.
 */
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

/**
 * @brief Configure locale information before creating any UI widgets.
 */
MyApp::AppResult MyApp::initializeLocale()
{
    if (m_locale.Init(wxLANGUAGE_DEFAULT, wxLOCALE_LOAD_DEFAULT))
    {
        util::Logger::Debug("Locale initialized successfully");
        return AppResult::Ok(std::monostate {});
    }

    return AppResult::Err(error::Error(error::ErrorCode::RuntimeError,
        "Failed to initialize locale, using default", false));
}