#include "xml/XmlParser.hpp"
#include <wx/xml/xml.h>
#include "parser/wxStdInputStreamAdapter.hpp"
#include "db/LogEvent.hpp"
#include <memory>
#include <sstream>
#include <iostream>
#include <fstream> // Include this header for std::ifstream

namespace parser
{

  XmlParser::XmlParser() : IDataParser(), m_currentProgress(0), m_totalProgress(0)
  {
  }

  void XmlParser::ParseData(const std::string &filepath)
  {
    // Open the file
    std::ifstream input(filepath);

    if (!input.is_open())
    {
      std::cerr << "Failed to open file: " << filepath << std::endl;
      return;
    }

    // Parse the data
    ParseData(input);

    input.close();
  }
  void XmlParser::ParseData(std::istream &input)
  {

    // Wrap std::istream in a wxStreamInputStream
    wxStdInputStreamAdapter wxStream(input);
    // Create a wxXmlDocument object
    wxXmlDocument xmlDoc;
    if (!xmlDoc.Load(wxStream))
    {
      std::cerr << "Failed to load XML data." << std::endl;
      return;
    }

    // Get the root element
    wxXmlNode *root = xmlDoc.GetRoot();
    if (root->GetName() != "events")
    {
      std::cerr << "Invalid root element. Expected <events>." << std::endl;
      return;
    }

    // Iterate through the child nodes
    wxXmlNode *child = root->GetChildren();
    int id = 0;
    while (child)
    {
      if (child->GetName() == "event")
      {
        // Process each <event> element
        // For example, you can extract attributes or child elements here
        wxString eventContent = child->GetNodeContent();
        std::cout << "Event: " << eventContent.ToStdString() << std::endl;
        wxXmlNode *item = child->GetChildren();
        db::LogEvent::EventItems eventItems;
        while (item)
        {
          eventItems.push_back({item->GetName().ToStdString(), item->GetNodeContent().ToStdString()});
          item = item->GetNext();
        }
        // Notify observers
        NewEventNotification({id, std::move(eventItems)});
        m_currentProgress = GetCurrentProgress();
        SendProgress();
      }

      child = child->GetNext();
      id++;
    }
  }

  uint32_t XmlParser::GetCurrentProgress() const
  {
    return m_currentProgress;
  }

  uint32_t XmlParser::GetTotalProgress() const
  {
    return 100u; // 100% progress
  }

} // namespace parser
