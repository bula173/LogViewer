#ifndef MYAPP_HPP
#define MYAPP_HPP

#include <wx/wx.h>

class MyApp : public wxApp
{
  public:
    MyApp() = default;
    virtual ~MyApp() = default;
    virtual bool OnInit();

  private:
    void setupLogging();
    void setupConfig();
};

#endif // MYAPP_HPP
