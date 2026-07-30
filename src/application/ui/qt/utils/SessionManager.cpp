#include "SessionManager.hpp"
#include "Config.hpp"
#include "Logger.hpp"

#include <filesystem>
#include <fstream>
#include <chrono>

namespace ui::qt::utils
{

using json = nlohmann::json;

SessionManager& SessionManager::getInstance()
{
    static SessionManager instance;
    return instance;
}

void SessionManager::initialize()
{
    // Set up session file paths
    const auto& configPath = config::GetConfig().GetConfigFilePath();
    const auto appDir = std::filesystem::path(configPath).parent_path();

    m_sessionFilePath = (appDir / "session.json").string();
    m_crashRecoveryFilePath = (appDir / "session.crash.json").string();

    // Check if crash recovery is available
    m_hasCrashRecovery = std::filesystem::exists(m_crashRecoveryFilePath);

    if (m_hasCrashRecovery)
    {
        util::Logger::Warn("[SessionManager] Previous crash detected - recovery available at {}",
            m_crashRecoveryFilePath);
    }

    util::Logger::Info("[SessionManager] Session manager initialized");
    util::Logger::Info("[SessionManager] Session path: {}", m_sessionFilePath);
}

bool SessionManager::hasCrashRecovery() const
{
    return m_hasCrashRecovery;
}

SessionManager::SessionState SessionManager::getCrashRecoveryState() const
{
    SessionState state;

    try
    {
        std::ifstream file(m_crashRecoveryFilePath);
        if (!file.is_open())
            return state;

        json j;
        file >> j;
        file.close();

        if (j.contains("state"))
        {
            const auto& s = j["state"];
            state.lastOpenedFile = s.value("lastOpenedFile", std::string{});
            state.lastScrollPosition = s.value("lastScrollPosition", 0);
            state.lastSelectedEvent = s.value("lastSelectedEvent", 0);

            if (s.contains("recentFiles") && s["recentFiles"].is_array())
            {
                state.recentFiles = s["recentFiles"].get<std::vector<std::string>>();
            }

            if (s.contains("activeFilters") && s["activeFilters"].is_array())
            {
                state.activeFilters = s["activeFilters"].get<std::vector<std::string>>();
            }
        }

        util::Logger::Info("[SessionManager] Loaded crash recovery state");
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[SessionManager] Failed to load crash recovery: {}", e.what());
    }

    return state;
}

void SessionManager::saveSession(const SessionState& state)
{
    try
    {
        json j;
        j["timestamp"] = std::chrono::system_clock::now().time_since_epoch().count();
        j["version"] = "1.0";

        j["state"]["lastOpenedFile"] = state.lastOpenedFile;
        j["state"]["lastScrollPosition"] = state.lastScrollPosition;
        j["state"]["lastSelectedEvent"] = state.lastSelectedEvent;
        j["state"]["recentFiles"] = state.recentFiles;
        j["state"]["activeFilters"] = state.activeFilters;
        j["state"]["columnWidths"] = state.columnWidths;

        // Save to regular session file
        std::ofstream file(m_sessionFilePath);
        if (file.is_open())
        {
            file << j.dump(2);
            file.close();
            util::Logger::Debug("[SessionManager] Saved session state");
        }

        // Also save to crash recovery file
        std::ofstream crashFile(m_crashRecoveryFilePath);
        if (crashFile.is_open())
        {
            crashFile << j.dump(2);
            crashFile.close();
        }
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[SessionManager] Failed to save session: {}", e.what());
    }
}

SessionManager::SessionState SessionManager::loadSession()
{
    SessionState state;

    try
    {
        std::ifstream file(m_sessionFilePath);
        if (!file.is_open())
            return state;

        json j;
        file >> j;
        file.close();

        if (j.contains("state"))
        {
            const auto& s = j["state"];
            state.lastOpenedFile = s.value("lastOpenedFile", std::string{});
            state.lastScrollPosition = s.value("lastScrollPosition", 0);
            state.lastSelectedEvent = s.value("lastSelectedEvent", 0);

            if (s.contains("recentFiles") && s["recentFiles"].is_array())
            {
                state.recentFiles = s["recentFiles"].get<std::vector<std::string>>();
            }

            if (s.contains("activeFilters") && s["activeFilters"].is_array())
            {
                state.activeFilters = s["activeFilters"].get<std::vector<std::string>>();
            }

            if (s.contains("columnWidths") && s["columnWidths"].is_object())
            {
                state.columnWidths = s["columnWidths"].get<std::map<std::string, int>>();
            }
        }

        util::Logger::Info("[SessionManager] Loaded session state");
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[SessionManager] Failed to load session: {}", e.what());
    }

    return state;
}

void SessionManager::clearCrashFlag()
{
    try
    {
        // Delete crash recovery file on successful shutdown
        if (std::filesystem::exists(m_crashRecoveryFilePath))
        {
            std::filesystem::remove(m_crashRecoveryFilePath);
            m_hasCrashRecovery = false;
            util::Logger::Info("[SessionManager] Cleared crash recovery flag");
        }
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[SessionManager] Failed to clear crash flag: {}", e.what());
    }
}

void SessionManager::setCrashFlag()
{
    // Crash flag is automatically set by auto-save functionality
    m_hasCrashRecovery = true;
}

void SessionManager::autoSave(const SessionState& state)
{
    if (!m_autoSaveEnabled)
        return;

    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
        now - m_lastAutoSaveTime).count();

    if (elapsed > kAutoSaveIntervalMs)
    {
        saveSession(state);
        m_lastAutoSaveTime = now;
    }
}

}  // namespace ui::qt::utils
