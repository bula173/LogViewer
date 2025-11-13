#ifndef MYAPP_HPP
#define MYAPP_HPP

#include <wx/wx.h>
#include <wx/intl.h>

// third party
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

class MyApp : public wxApp
{
  public:
    MyApp() = default;
    virtual ~MyApp() = default;
    virtual bool OnInit();

  private:
    void setupLogging();
    void setupConfig();
    void ChangeLogLevel();

  private:
    const std::string m_appName {"LogViewer"};
    wxLocale m_locale; // Locale object for proper Unicode/Polish support
};

#endif // MYAPP_HPP
