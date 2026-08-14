#pragma once

#include <string>
#include <vector>
#include <map>
#include <nlohmann/json.hpp>

namespace ui::qt::utils
{

/**
 * @brief Manages session state (open files, filters, scroll position, etc.)
 *
 * Automatically saves state periodically and on shutdown.
 * Allows recovery from crashes with user confirmation.
 */
class SessionManager
{
  public:
    struct SessionState
    {
        std::vector<std::string> recentFiles;  // Recently opened files
        std::string lastOpenedFile;            // Last active file
        int lastScrollPosition {0};            // Scroll position in events table
        int lastSelectedEvent {0};             // Currently selected event
        std::map<std::string, int> columnWidths;  // Column width settings
        std::vector<std::string> activeFilters;   // Active filter names
        bool autoRecoveryEnabled {true};       // Auto-recover from crashes
    };

    static SessionManager& getInstance();

    /// Initialize session manager
    void initialize();

    /// Check if crash recovery is available
    bool hasCrashRecovery() const;

    /// Get crash recovery state
    SessionState getCrashRecoveryState() const;

    /// Save current session state
    void saveSession(const SessionState& state);

    /// Load last session state
    SessionState loadSession();

    /// Clear crash recovery flag (session completed successfully)
    void clearCrashFlag();

    /// Set crash recovery flag (session interrupted abnormally)
    void setCrashFlag();

    /// Auto-save session periodically (call from main event loop)
    void autoSave(const SessionState& state);

    /// Enable/disable auto-save
    void setAutoSaveEnabled(bool enabled) { m_autoSaveEnabled = enabled; }
    bool isAutoSaveEnabled() const { return m_autoSaveEnabled; }

    /// Get session file path
    std::string getSessionFilePath() const { return m_sessionFilePath; }

    /// Get crash recovery file path
    std::string getCrashRecoveryFilePath() const { return m_crashRecoveryFilePath; }

  private:
    SessionManager() = default;

    std::string m_sessionFilePath;
    std::string m_crashRecoveryFilePath;
    bool m_autoSaveEnabled {true};
    bool m_hasCrashRecovery {false};
    std::chrono::steady_clock::time_point m_lastAutoSaveTime;
    static constexpr std::chrono::milliseconds kAutoSaveInterval {30000};  // 30 seconds
};

}  // namespace ui::qt::utils
