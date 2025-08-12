/**
 * @file Error.hpp
 * @brief Error handling utilities.
 */

#pragma once

#include <atomic>
#include <spdlog/spdlog.h>
#include <stdexcept>
#include <string>

// Add the missing wx headers for wxTheApp and wxIsMainThread
#include <wx/app.h> // wxTheApp
#include <wx/msgdlg.h>
#include <wx/string.h>
#include <wx/thread.h> // wxIsMainThread

namespace error
{

inline void ShowError(const wxString& message)
{
    wxMessageBox(message, "Error", wxOK | wxICON_ERROR);
}
inline void ShowError(const std::string& message)
{
    ShowError(wxString::FromUTF8(message.c_str()));
}
inline void ShowError(const std::wstring& message)
{
    ShowError(wxString(message));
}

// Global toggle for showing dialogs (default: true)
inline std::atomic<bool>& DialogsEnabledFlag()
{
    static std::atomic<bool> flag {true};
    return flag;
}
inline void SetShowDialogs(bool enabled)
{
    DialogsEnabledFlag().store(enabled);
}
inline bool GetShowDialogs()
{
    return DialogsEnabledFlag().load();
}
inline bool CanShowDialogs()
{
    // Only show if enabled, app exists, and we're on the main thread
    return GetShowDialogs() && wxTheApp != nullptr && wxIsMainThread();
}

class Error : public std::runtime_error
{
  public:
    explicit Error(const char* message, bool showMsgBox = true)
        : Error(std::string(message), showMsgBox)
    {
    }

    explicit Error(const std::string& message, bool showMsgBox = true)
        : std::runtime_error(message)
    {
        if (showMsgBox && CanShowDialogs())
            ShowError(message);
        spdlog::error("Application Error: {}", message);
    }

    explicit Error(const wxString& message, bool showMsgBox = true)
        : std::runtime_error(message.ToStdString())
    {
        if (showMsgBox && CanShowDialogs())
            ShowError(message);
        spdlog::error("Application Error: {}", message.ToStdString());
    }
};

} // namespace error