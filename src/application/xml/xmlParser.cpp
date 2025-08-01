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
        state->eventItems.reserve(10); // Reserve typical event size
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
        // Add event to batch instead of immediate notification
        state->eventBatch.emplace_back(
            state->eventId++, std::move(state->eventItems));
        state->insideEvent = false;

        // Send batch every N events
        if (state->eventBatch.size() >= 500)
        { // Larger batches
            state->parser->SendEventBatch(std::move(state->eventBatch));
            state->eventBatch.clear();
        }
    }
    else if (state->insideEvent && elementName == state->currentElement)
    {
        state->eventItems.emplace_back(
            std::move(state->currentElement), std::move(state->currentText));
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

    // Update progress calculation
    uint32_t newProgress = (state->totalBytes > 0)
        ? static_cast<uint32_t>(
              (static_cast<double>(state->bytesProcessed) / state->totalBytes) *
              100)
        : 100;

    // Only send progress updates when percentage changes OR every N bytes
    if (newProgress != state->parser->m_currentProgress ||
        (state->bytesProcessed - state->lastProgressBytes) > 1024 * 100)
    { // Every 100KB

        state->parser->m_currentProgress = newProgress;
        state->parser->NotifyProgressUpdated();
        state->lastProgressBytes = state->bytesProcessed;

        spdlog::debug("XmlParser::ParseData progress: {}% ({}/{} bytes)",
            newProgress, state->bytesProcessed, state->totalBytes);
    }
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

    // Initialize parser state
    ParserState state;
    state.parser = this;

    // Get file size for progress tracking
    input.seekg(0, std::ios::end);
    state.totalBytes = input.tellg();
    input.seekg(0, std::ios::beg);

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

    // Stream parsing with buffer
    const size_t bufferSize = 64 * 1024; // 64KB chunks
    std::vector<char> buffer(bufferSize);

    while (input.good())
    {
        input.read(buffer.data(), bufferSize);
        std::streamsize bytesRead = input.gcount();

        if (bytesRead > 0)
        {
            state.bytesProcessed += bytesRead;

            if (XML_Parse(parser, buffer.data(), static_cast<int>(bytesRead),
                    input.eof() ? XML_TRUE : XML_FALSE) == XML_STATUS_ERROR)
            {
                spdlog::error("XmlParser::ParseData failed to parse XML: {}",
                    XML_ErrorString(XML_GetErrorCode(parser)));
                break;
            }
        }
    }

    // Send final batch if any
    if (!state.eventBatch.empty())
    {
        SendEventBatch(std::move(state.eventBatch)); // This should now work
    }

    XML_ParserFree(parser);

    spdlog::debug("XmlParser::ParseData finished parsing. Processed: {}",
        state.bytesProcessed);
    NotifyProgressUpdated(); // Ensure the view is up-to-date at the end
}

uint32_t XmlParser::GetCurrentProgress() const
{
    return m_currentProgress;
}

uint32_t XmlParser::GetTotalProgress() const
{
    return 100u; // 100% progress
}

void XmlParser::SendEventBatch(
    std::vector<std::pair<int, db::LogEvent::EventItems>>&& eventBatch)
{
    // Process each event in the batch
    for (auto& [eventId, items] : eventBatch)
    {
        // Create an event and notify observers
        db::LogEvent event(eventId, std::move(items));
        NotifyNewEvent(std::move(event)); // Use the base class method
    }
}

} // namespace parser
