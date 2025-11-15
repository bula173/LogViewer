#ifndef MYAPP_HPP
#define MYAPP_HPP

#include <wx/wx.h>
#include <wx/intl.h>

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
