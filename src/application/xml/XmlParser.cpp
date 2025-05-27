// internal
#include "xml/xmlParser.hpp"
#include "config/Config.hpp"
#include "db/LogEvent.hpp"
#include "parser/wxStdInputStreamAdapter.hpp"

// wxWidgets
#include <wx/xml/xml.h>

// std
#include <fstream> // Include this header for std::ifstream
#include <iostream>
#include <memory>
#include <sstream>
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
    spdlog::debug("XmlParser::ParseData called with filepath: {}", filepath);
    std::ifstream input(filepath);

    if (!input.is_open())
    {
        spdlog::error("XmlParser::ParseData failed to open file: {}", filepath);
        return;
    }

    ParseData(input);

    input.close();
}

void XmlParser::ParseData(std::istream& input)
{
    spdlog::debug("XmlParser::ParseData called with istream");
    wxStdInputStreamAdapter wxStream(input);
    wxXmlDocument xmlDoc;
    auto& config = config::GetConfig();

    if (!xmlDoc.Load(wxStream))
    {
        spdlog::error("XmlParser::ParseData failed to load XML data.");
        return;
    }

    wxXmlNode* root = xmlDoc.GetRoot();
    spdlog::debug("XmlParser::ParseData root element: {}", root ? root->GetName().ToStdString() : "nullptr");
    if (!root || root->GetName() != config.xmlRootElement)
    {
        spdlog::error("XmlParser::ParseData invalid root element. Expected <{}>.", config.xmlRootElement);
        return;
    }

    // Count total events first
    size_t totalEvents = 0;
    for (wxXmlNode* node = root->GetChildren(); node; node = node->GetNext())
        if (node->GetName() == config.xmlEventElement)
            ++totalEvents;
    spdlog::debug("XmlParser::ParseData total events to parse: {}", totalEvents);

    // Now, parse and update progress
    wxXmlNode* child = root->GetChildren();
    int id = 0;
    size_t processed = 0;
    while (child)
    {
        if (child->GetName() == config.xmlEventElement)
        {
            spdlog::debug("XmlParser::ParseData parsing event id: {}", id);
            wxXmlNode* item = child->GetChildren();
            db::LogEvent::EventItems eventItems;
            while (item)
            {
                spdlog::debug("XmlParser::ParseData found item: {} = {}", item->GetName().ToStdString(), item->GetNodeContent().ToStdString());
                eventItems.push_back({item->GetName().ToStdString(),
                    item->GetNodeContent().ToStdString()});
                item = item->GetNext();
            }
            NewEventNotification({id, std::move(eventItems)});
            ++processed;
            m_currentProgress =
                (totalEvents > 0) ? (processed * 100) / totalEvents : 100;
            SendProgress();
        }
        child = child->GetNext();
        id++;
    }
    spdlog::debug("XmlParser::ParseData finished parsing. Processed: {}", processed);
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
