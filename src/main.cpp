
#include <wx/wx.h>

#include "main_windows.hpp"
class MyApp : public wxApp
{
public:
    virtual bool OnInit();
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    MainWindows *frame = new MainWindows("LogViewer", wxPoint(50, 50), wxSize(600, 800));
    frame->Show(true);
    return true;
}
