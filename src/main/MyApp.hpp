#ifndef MYAPP_HPP
#define MYAPP_HPP

#include "error/Error.hpp"
#include "util/Result.hpp"

#include <variant>
#include <wx/intl.h>
#include <wx/wx.h>

/**
 * @class MyApp
 * @brief wxWidgets application bootstrapper responsible for initializing
 *        logging, configuration, and the main window.
 */
class MyApp : public wxApp
{
  public:
    MyApp() = default;
    ~MyApp() override = default;

    /**
     * @brief wxWidgets entry point. Creates the locale, config, and main window.
     * @return true when initialization succeeds.
     */
    bool OnInit() override;

  private:
    using AppResult = util::Result<std::monostate, error::Error>;

    [[nodiscard]] AppResult setupLogging();
    [[nodiscard]] AppResult setupConfig();
    [[nodiscard]] AppResult initializeLocale();
    void ChangeLogLevel();

  private:
    const std::string m_appName {"LogViewer"};
    wxLocale m_locale; ///< Locale object for proper Unicode/Polish support
};

#endif // MYAPP_HPP
