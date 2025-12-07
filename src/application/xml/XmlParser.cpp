// internal
#include "xml/xmlParser.hpp"
#include "config/Config.hpp"
#include "db/LogEvent.hpp"
#include "error/Error.hpp"
#include "util/Logger.hpp"
#include "util/Result.hpp"

// std
#include <cstring>
#include <fstream>
#include <vector>
namespace
{
constexpr unsigned char UTF8_BOM[] = {0xEF, 0xBB, 0xBF};

using ReadResult = util::Result<std::streamsize, error::Error>;

ReadResult ReadChunk(std::istream& input, std::vector<char>& buffer)
{
    input.read(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if (input.bad())
    {
        return ReadResult::Err(error::Error(error::ErrorCode::IOError,
            "XmlParser::ParseData encountered stream read error", false));
    }
    return ReadResult::Ok(input.gcount());
}

void StripUtf8BomIfPresent(parser::ParserState& state,
    std::vector<char>& buffer, std::streamsize& bytesRead)
{
    if (state.bytesProcessed > 0 || bytesRead < 3)
        return;

    if (static_cast<unsigned char>(buffer[0]) == UTF8_BOM[0] &&
        static_cast<unsigned char>(buffer[1]) == UTF8_BOM[1] &&
        static_cast<unsigned char>(buffer[2]) == UTF8_BOM[2])
    {
        util::Logger::Debug("XmlParser::ParseData detected UTF-8 BOM; skipping");
        std::memmove(buffer.data(), buffer.data() + 3,
            static_cast<size_t>(bytesRead - 3));
        bytesRead -= 3;
        state.bytesProcessed += 3; // account for skipped bytes
    }
}
} // namespace

namespace parser
{

// expat static handler functions

static void StartElementHandler(
    void* userData, const XML_Char* name, const XML_Char** atts)
{
    ParserState* state = static_cast<ParserState*>(userData);
    const auto& config = config::GetConfig();
    std::string elementName(name);

    if (!state->insideRoot)
    {
        if (elementName == config.xmlRootElement)
        {
            util::Logger::Debug(
                "XmlParser::ParseData found root element: {}", elementName);
            state->insideRoot = true;
        }
        else
        {
            util::Logger::Warn(
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
        
        // Parse attributes if present (e.g., <event id="1" timestamp="..." />)
        if (atts != nullptr)
        {
            for (int i = 0; atts[i]; i += 2)
            {
                std::string attrName(atts[i]);
                std::string attrValue(atts[i + 1]);
                state->eventItems.emplace_back(std::move(attrName), std::move(attrValue));
            }
        }
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
        state->currentText.append(s, static_cast<size_t>(len));
    }

    state->bytesProcessed = static_cast<size_t>(
        XML_GetCurrentByteIndex(state->parserHandle));

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

        util::Logger::Trace("XmlParser::ParseData progress: {}% ({}/{} bytes)",
            newProgress, state->bytesProcessed, state->totalBytes);
    }
}

XmlParser::XmlParser()
    : IDataParser()
    , m_currentProgress(0)
{
}

/**
 * @brief Loads and parses XML data from a file path.
 */
void XmlParser::ParseData(const std::filesystem::path& filepath)
{
    util::Logger::Debug(
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

/**
 * @brief Parse XML data from an arbitrary input stream.
 */
void XmlParser::ParseData(std::istream& input)
{
    util::Logger::Debug("XmlParser::ParseData called with istream");

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

    // Force UTF-8 parsing to keep Polish characters intact even if files lack
    // explicit encoding declarations.
    XML_Parser parser = XML_ParserCreate("UTF-8");
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
        bool bomProcessed = false;

        for (;;)
        {
            auto chunkResult = ReadChunk(input, buffer);
            if (chunkResult.isErr())
            {
                throw chunkResult.error();
            }

            std::streamsize bytesRead = chunkResult.unwrap();

            if (bytesRead == 0)
            {
                // No progress: break to avoid hang. Finalize below.
                // If fail() without eof(), clear and break to finalize.
                if (input.fail() && !input.eof())
                    input.clear();
                break;
            }

            if (!bomProcessed)
            {
                StripUtf8BomIfPresent(state, buffer, bytesRead);
                bomProcessed = true;
            }

            state.bytesProcessed += static_cast<uint64_t>(bytesRead);

            const auto isFinal = static_cast<XML_Bool>(input.eof() ? 1 : 0);
            if (isFinal == static_cast<XML_Bool>(1))
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
            if (XML_Parse(parser, nullptr, 0, static_cast<XML_Bool>(1)) == XML_STATUS_ERROR)
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

        util::Logger::Debug("XmlParser::ParseData finished. Processed: {}",
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
        throw error::Error(std::string("XmlParser::ParseData (istream) failed: ") +
            e.what());
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
