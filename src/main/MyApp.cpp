// internal
#include "MyApp.hpp"
#include "config/Config.hpp"
#include "gui/MainWindow.hpp"
#include "main/version.h"
// third party
#include <spdlog/spdlog.h>
// std
#include <filesystem>

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    spdlog::info("Starting LogViewer application");

#ifdef NDEBUG
    spdlog::info("Release build");
    spdlog::set_level(spdlog::level::warn); // Release: warnings and errors only
#else
    spdlog::info("Debug build");
    spdlog::set_level(
        spdlog::level::debug); // Debug: show debug/info/warn/error
#endif
    if (!wxApp::OnInit())
        return false;

    spdlog::info(
        "Current working dir: {}", std::filesystem::current_path().string());

    const auto& version = Version::current();
    auto& config = config::GetConfig();

    // Get current working directory (assumes running from project root or
    // build/)
    std::filesystem::path cwd = std::filesystem::current_path();
    std::filesystem::path configPath = cwd / "config.json";

    config.SetConfigFilePath(configPath.string());
    config.LoadConfig();
    gui::MainWindow* frame =
        new gui::MainWindow("LogViewer " + version.asShortStr(),
            wxPoint(50, 50), wxSize(1000, 600), version);
    SetTopWindow(frame);
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
// This is the main application class. It initializes the wxWidgets library