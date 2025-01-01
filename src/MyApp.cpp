#include "GUI/main_window.hpp"
#include "MyApp.hpp"

#include <glog/logging.h>

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
  google::InitGoogleLogging(argv[0]);
  LOG(INFO) << "Starting application";
  gui::MainWindow *frame = new gui::MainWindow("LogViewer", wxPoint(50, 50), wxSize(1000, 600));
  frame->Show(true);
  return true;
}
