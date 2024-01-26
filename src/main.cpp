
#include <wx/wx.h>

#include "GUI/main_window.hpp"

class MyApp : public wxApp
{
public:
    virtual bool OnInit();
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    gui::MainWindow *frame = new gui::MainWindow("LogViewer", wxPoint(50, 50), wxSize(600, 800));
    frame->Show(true);
    return true;
}
