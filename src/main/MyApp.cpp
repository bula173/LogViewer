#include "MyApp.hpp"
#include "gui/MainWindow.hpp"
#include "main/version.h"

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    if (!wxApp::OnInit())
        return false;

    const auto& version = Version::current();
    gui::MainWindow* frame =
        new gui::MainWindow("LogViewer " + version.asShortStr(),
            wxPoint(50, 50), wxSize(1000, 600), version);
    SetTopWindow(frame);
    frame->Show(true);
    return true;
}
// This is the main application class. It initializes the wxWidgets library