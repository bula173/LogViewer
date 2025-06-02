// internal
#include "xml/xmlParser.hpp"
#include "config/Config.hpp"
#include "db/LogEvent.hpp"

// std
#include <fstream> // Include this header for std::ifstream
#include <iostream>
#include <memory>
#include <sstream>
namespace parser
{

// expat static handler functions

static void StartElementHandler(
    void* userData, const XML_Char* name, const XML_Char**)
{
    ParserState* state = static_cast<ParserState*>(userData);
    // Use config from outer function
    const auto& config = config::GetConfig();
    std::string elementName(name);
    if (!state->insideRoot)
    {
        if (elementName == config.xmlRootElement)
        {
            spdlog::debug(
                "XmlParser::ParseData found root element: {}", elementName);
            state->insideRoot = true;
        }
        else
        {
            spdlog::warn("XmlParser::ParseData unexpected element: {} "
                         "outside root",
                elementName);
        }
        {
            state->insideRoot = true;
        }
        return;
    }
    if (elementName == config.xmlEventElement)
    {
        state->insideEvent = true;
        state->eventItems.clear();
    }
    else if (state->insideEvent)
    {
        state->currentElement = elementName;
        state->currentText.clear();
    }
}

static void EndElementHandler(void* userData, const XML_Char* name)
{
    ParserState* state = static_cast<ParserState*>(userData);
    const auto& config = config::GetConfig();
    std::string elementName(name);
    if (!state->insideRoot)
    {
        return;
    }
    if (elementName == config.xmlEventElement)
    {
        state->parser->NewEventNotification(
            {state->eventId++, std::move(state->eventItems)});
        state->insideEvent = false;
    }
    else if (state->insideEvent && elementName == state->currentElement)
    {
        state->eventItems.emplace_back(
            state->currentElement, state->currentText);
        state->currentElement.clear();
        state->currentText.clear();
    }
}

static void CharacterDataHandler(void* userData, const XML_Char* s, int len)
{
    ParserState* state = static_cast<ParserState*>(userData);
    if (state->insideEvent && !state->currentElement.empty())
    {
        state->currentText.append(s, len);
    }
    state->bytesProcessed = XML_GetCurrentByteIndex(state->parserHandle);
    state->parser->m_currentProgress = (state->totalBytes > 0)
        ? static_cast<uint32_t>(
              (static_cast<double>(state->bytesProcessed) / state->totalBytes) *
              100)
        : 100;
    spdlog::debug("XmlParser::ParseData progress: {}/{} bytes processed",
        state->bytesProcessed, state->totalBytes);
    state->parser->SendProgress();
}

XmlParser::XmlParser()
    : IDataParser()
    , m_currentProgress(0)
{
}

// Loads and parses XML file using Expat
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

    std::stringstream buffer;
    buffer << input.rdbuf();
    std::string xmlContent = buffer.str();
    input.clear();
    input.seekg(0);

    // Initialize parser state
    ParserState state;
    state.totalBytes = xmlContent.size();
    state.parser = this;
    state.parserHandle = nullptr;

    XML_Parser parser = XML_ParserCreate(NULL);
    if (!parser)
    {
        spdlog::error("XmlParser::ParseData failed to create XML parser");
        return;
    }
    state.parserHandle = parser;

    XML_SetUserData(parser, &state);
    XML_SetStartElementHandler(parser, StartElementHandler);
    XML_SetEndElementHandler(parser, EndElementHandler);
    XML_SetCharacterDataHandler(parser, CharacterDataHandler);

    if (XML_Parse(parser, xmlContent.c_str(),
            static_cast<int>(xmlContent.size()), XML_TRUE) == XML_STATUS_ERROR)
    {
        spdlog::error("XmlParser::ParseData failed to parse XML: {}",
            XML_ErrorString(XML_GetErrorCode(parser)));
        XML_ParserFree(parser);
        return;
    }

    XML_ParserFree(parser);

    spdlog::debug("XmlParser::ParseData finished parsing. Processed: {}",
        state.bytesProcessed);
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
