#include "xml/xmlParser.hpp"
#include "db/LogEvent.hpp"
#include "parser/wxStdInputStreamAdapter.hpp"
#include <fstream> // Include this header for std::ifstream
#include <iostream>
#include <memory>
#include <sstream>
#include <wx/xml/xml.h>

namespace parser
{

XmlParser::XmlParser()
    : IDataParser()
    , m_currentProgress(0)
    , m_totalProgress(0)
{
}

void XmlParser::ParseData(const std::string& filepath)
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
void XmlParser::ParseData(std::istream& input)
{
    wxStdInputStreamAdapter wxStream(input);
    wxXmlDocument xmlDoc;
    if (!xmlDoc.Load(wxStream))
    {
        std::cerr << "Failed to load XML data." << std::endl;
        return;
    }

    wxXmlNode* root = xmlDoc.GetRoot();
    if (root->GetName() != "events")
    {
        std::cerr << "Invalid root element. Expected <events>." << std::endl;
        return;
    }

    // Count total events first
    size_t totalEvents = 0;
    for (wxXmlNode* node = root->GetChildren(); node; node = node->GetNext())
        if (node->GetName() == "event")
            ++totalEvents;

    // Now, parse and update progress
    wxXmlNode* child = root->GetChildren();
    int id = 0;
    size_t processed = 0;
    while (child)
    {
        if (child->GetName() == "event")
        {
            wxXmlNode* item = child->GetChildren();
            db::LogEvent::EventItems eventItems;
            while (item)
            {
                eventItems.push_back({item->GetName().ToStdString(),
                    item->GetNodeContent().ToStdString()});
                item = item->GetNext();
            }
            NewEventNotification({id, std::move(eventItems)});
            // Calculate progress as percentage
            ++processed;
            m_currentProgress =
                (totalEvents > 0) ? (processed * 100) / totalEvents : 100;
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
