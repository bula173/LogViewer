#ifndef MYAPP_HPP
#define MYAPP_HPP

#include <wx/wx.h>

class MyApp : public wxApp
{
  public:
    MyApp() = default;
    virtual ~MyApp() = default;
    virtual bool OnInit();
};

#endif // MYAPP_HPP
