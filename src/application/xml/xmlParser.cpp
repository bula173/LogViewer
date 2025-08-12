// internal
#include "xml/xmlParser.hpp"
#include "config/Config.hpp"
#include "db/LogEvent.hpp"
#include "error/Error.hpp"

// third-party
#include <spdlog/spdlog.h>

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
            spdlog::warn(
                "XmlParser::ParseData unexpected element: {} outside root",
                elementName);
            // Do NOT flip insideRoot here; wait for the correct root.
        }
        return;
    }

    if (elementName == config.xmlEventElement)
    {
        state->insideEvent = true;
        state->eventItems.clear();
        state->eventItems.reserve(10);
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
            state->parser->NotifyNewEventBatch(std::move(state->eventBatch));
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
void XmlParser::ParseData(const std::filesystem::path& filepath)
{
    spdlog::debug(
        "XmlParser::ParseData called with filepath: {}", filepath.string());
    std::ifstream input(filepath, std::ios::binary);

    if (!input.is_open())
    {
        throw error::Error(
            "XmlParser::ParseData failed to open file: " + filepath.string());
    }

    ParseData(input);

    input.close();
}

void XmlParser::ParseData(std::istream& input)
{
    spdlog::debug("XmlParser::ParseData called with istream");

    if (!input.good())
        throw error::Error(
            "XmlParser::ParseData: input stream is not readable");

    ParserState state;
    state.parser = this;

    // Try to determine total size (optional)
    {
        std::streampos cur = input.tellg();
        if (cur != std::streampos(-1))
        {
            input.seekg(0, std::ios::end);
            std::streampos end = input.tellg();
            if (end != std::streampos(-1))
                state.totalBytes = static_cast<uint64_t>(end);
            input.seekg(cur); // restore original pos
        }
        else
        {
            state.totalBytes = 0;
        }
    }

    XML_Parser parser = XML_ParserCreate(nullptr);
    if (!parser)
        throw error::Error("XmlParser::ParseData failed to create XML parser");

    state.parserHandle = parser;
    XML_SetUserData(parser, &state);
    XML_SetStartElementHandler(parser, StartElementHandler);
    XML_SetEndElementHandler(parser, EndElementHandler);
    XML_SetCharacterDataHandler(parser, CharacterDataHandler);

    try
    {
        constexpr size_t bufferSize = 64 * 1024;
        std::vector<char> buffer(bufferSize);
        bool finalSent = false;

        for (;;)
        {
            input.read(buffer.data(), bufferSize);
            const std::streamsize bytesRead = input.gcount();

            if (bytesRead == 0)
            {
                // No progress: break to avoid hang. Finalize below.
                if (input.bad())
                    throw error::Error("XmlParser::ParseData I/O error while "
                                       "reading input stream");
                // If fail() without eof(), clear and break to finalize.
                if (input.fail() && !input.eof())
                    input.clear();
                break;
            }

            state.bytesProcessed += static_cast<uint64_t>(bytesRead);

            const auto isFinal = input.eof() ? XML_TRUE : XML_FALSE;
            if (isFinal == XML_TRUE)
                finalSent = true;

            if (XML_Parse(parser, buffer.data(), static_cast<int>(bytesRead),
                    isFinal) == XML_STATUS_ERROR)
            {
                const auto code = XML_GetErrorCode(parser);
                const auto line = XML_GetCurrentLineNumber(parser);
                const auto col = XML_GetCurrentColumnNumber(parser);

                throw error::Error(
                    std::string("XmlParser::ParseData XML error: ") +
                    XML_ErrorString(code) + " at line " + std::to_string(line) +
                    ", column " + std::to_string(col));
            }

            if (input.eof())
                break;
        }

        // Finalize only if a final chunk wasn’t already sent.
        if (!finalSent)
        {
            if (XML_Parse(parser, nullptr, 0, XML_TRUE) == XML_STATUS_ERROR)
            {
                const auto code = XML_GetErrorCode(parser);
                if (code != XML_ERROR_FINISHED) // ignore double-finalize case
                {
                    const auto line = XML_GetCurrentLineNumber(parser);
                    const auto col = XML_GetCurrentColumnNumber(parser);

                    throw error::Error(
                        std::string(
                            "XmlParser::ParseData XML finalize error: ") +
                        XML_ErrorString(code) + " at line " +
                        std::to_string(line) + ", column " +
                        std::to_string(col));
                }
            }
        }

        if (!state.eventBatch.empty())
            NotifyNewEventBatch(std::move(state.eventBatch));

        spdlog::debug("XmlParser::ParseData finished. Processed: {}",
            state.bytesProcessed);
        NotifyProgressUpdated();
        XML_ParserFree(parser);
    }
    catch (const error::Error&)
    {
        XML_ParserFree(parser);
        throw;
    }
    catch (const std::exception& e)
    {
        XML_ParserFree(parser);
        throw error::Error(
            std::string("XmlParser::ParseData (istream) failed: ") + e.what());
    }
    catch (...)
    {
        XML_ParserFree(parser);
        throw error::Error(
            "XmlParser::ParseData (istream) failed due to unknown error");
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
