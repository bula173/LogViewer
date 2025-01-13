#include "gui/MainWindow.hpp"
#include "MyApp.hpp"

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    gui::MainWindow *frame = new gui::MainWindow("LogViewer", wxPoint(50, 50), wxSize(1000, 600));
    frame->Show(true);
    return true;
}
