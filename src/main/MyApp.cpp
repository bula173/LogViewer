#include "MyApp.hpp"
#include "gui/MainWindow.hpp"

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    if (!wxApp::OnInit())
        return false;

    gui::MainWindow* frame =
        new gui::MainWindow("LogViewer", wxPoint(50, 50), wxSize(1000, 600));
    frame->Show(true);
    return true;
}
