/**
 * @file CsvParser.cpp
 * @brief Implementation of CSV log file parser
 * @author LogViewer Development Team
 * @date 2025
 */

#include "CsvParser.hpp"
#include "LogEvent.hpp"
#include "Error.hpp"
#include "Logger.hpp"

#include <algorithm>
#include <fstream>
#include <sstream>

namespace parser
{

CsvParser::CsvParser()
    : IDataParser()
    , m_currentProgress(0)
    , m_totalProgress(0)
{
    util::Logger::Debug("CsvParser constructor called");
}

void CsvParser::ParseData(const std::filesystem::path& filepath)
{
    util::Logger::Debug("CsvParser::ParseData called with filepath: {}", filepath.string());

    std::ifstream input(filepath, std::ios::binary);
    if (!input.is_open())
    {
        throw error::Error(
            "CsvParser::ParseData failed to open file: " + filepath.string());
    }

    // Use a 1 MB read buffer to reduce system-call overhead on large files.
    constexpr size_t kReadBufSize = 1024 * 1024;
    std::vector<char> readBuf(kReadBufSize);
    input.rdbuf()->pubsetbuf(readBuf.data(),
                             static_cast<std::streamsize>(kReadBufSize));

    // Determine file size for progress tracking
    input.seekg(0, std::ios::end);
    m_totalProgress = static_cast<uint32_t>(input.tellg());
    input.seekg(0, std::ios::beg);
    m_currentProgress = 0;

    ParseData(input);
    input.close();
}

void CsvParser::ParseData(std::istream& input)
{
    util::Logger::Debug("CsvParser::ParseData called with istream");

    if (!input.good())
    {
        throw error::Error(
            "CsvParser::ParseData: input stream is not readable");
    }

    // Try to determine total size if not already set
    if (m_totalProgress == 0)
    {
        std::streampos cur = input.tellg();
        if (cur != std::streampos(-1))
        {
            input.seekg(0, std::ios::end);
            std::streampos end = input.tellg();
            if (end != std::streampos(-1))
            {
                m_totalProgress = static_cast<uint32_t>(end);
            }
            input.seekg(cur);
        }
    }

    std::vector<std::string> headers;
    std::string line;
    int eventId = 0;
    size_t lineNumber = 0;
    constexpr size_t BATCH_SIZE = 5000; // larger batch = fewer mutex + notify calls
    std::vector<std::pair<int, db::LogEvent::EventItems>> eventBatch;
    eventBatch.reserve(BATCH_SIZE);

    // Avoid calling tellg() on every line — it triggers a file-position sync
    // on every iteration which is very slow.  Instead, estimate bytes consumed
    // by accumulating line lengths (+ 1 for the newline stripped by getline).
    size_t bytesConsumed = 0;
    constexpr size_t kProgressReportEvery = 5000; // update every N events

    // Read and parse each line
    while (std::getline(input, line))
    {
        lineNumber++;
        bytesConsumed += line.size() + 1; // +1 for stripped newline

        // Skip empty lines
        if (line.empty() || line.find_first_not_of(" \t\r\n") == std::string::npos)
        {
            continue;
        }

        // Parse the line into fields
        std::vector<std::string> fields = ParseLine(line);

        if (fields.empty())
        {
            continue;
        }

        // First line might be headers
        if (lineNumber == 1 && m_hasHeaders)
        {
            headers = fields;
            util::Logger::Debug("CsvParser: Found {} headers", headers.size());
            
            // Normalize header names (trim and lowercase)
            for (auto& header : headers)
            {
                header = Trim(header);
                std::transform(header.begin(), header.end(), header.begin(),
                    [](unsigned char c) { return std::tolower(c); });
            }
            continue;
        }

        // If we don't have headers, create default ones
        if (headers.empty())
        {
            for (size_t i = 0; i < fields.size(); ++i)
            {
                headers.push_back("field" + std::to_string(i));
            }
        }

        // Create event from fields
        try
        {
            db::LogEvent::EventItems items = CreateEventFromFields(fields, headers, ++eventId);
            
            // Add to batch
            eventBatch.emplace_back(eventId, std::move(items));

            // Send batch when full
            if (eventBatch.size() >= BATCH_SIZE)
            {
                m_currentProgress = static_cast<uint32_t>(bytesConsumed);
                NotifyNewEventBatch(std::move(eventBatch));
                eventBatch.clear();
                eventBatch.reserve(BATCH_SIZE);

                if ((static_cast<size_t>(eventId) % kProgressReportEvery) == 0)
                    NotifyProgressUpdated();
            }
        }
        catch (const std::exception& e)
        {
            util::Logger::Warn("CsvParser: Failed to parse line {}: {}", 
                lineNumber, e.what());
        }
    }

    // Send remaining events in batch
    if (!eventBatch.empty())
    {
        m_currentProgress = static_cast<uint32_t>(bytesConsumed);
        NotifyNewEventBatch(std::move(eventBatch));
    }

    // Final progress update
    m_currentProgress = m_totalProgress;
    NotifyProgressUpdated();

    util::Logger::Info("CsvParser: Successfully parsed {} events from {} lines",
        eventId, lineNumber);
}

std::vector<std::string> CsvParser::ParseLine(const std::string& line)
{
    std::vector<std::string> fields;
    std::string_view lineView(line);
    size_t pos = 0;
    bool inQuotes = false;
    
    while (pos < lineView.length())
    {
        // Skip leading whitespace for unquoted fields
        if (!inQuotes)
        {
            while (pos < lineView.length() && (lineView[pos] == ' ' || lineView[pos] == '\t'))
                ++pos;
        }
        
        if (pos >= lineView.length())
            break;
            
        size_t fieldStart = pos;
        bool fieldHasQuotes = false;
        
        // Parse field
        while (pos < lineView.length())
        {
            char c = lineView[pos];
            
            if (c == '"')
            {
                if (!inQuotes)
                {
                    // Start of quoted field
                    inQuotes = true;
                    fieldHasQuotes = true;
                    fieldStart = pos + 1; // Skip opening quote
                }
                else if (pos + 1 < lineView.length() && lineView[pos + 1] == '"')
                {
                    // Escaped quote - skip it
                    ++pos;
                }
                else
                {
                    // End of quoted field
                    inQuotes = false;
                    size_t fieldEnd = pos;
                    pos++; // Skip closing quote
                    
                    // Skip whitespace after closing quote
                    while (pos < lineView.length() && (lineView[pos] == ' ' || lineView[pos] == '\t'))
                        ++pos;
                        
                    // Expect delimiter or end of line
                    if (pos >= lineView.length() || lineView[pos] == m_delimiter)
                    {
                        if (pos < lineView.length())
                            ++pos; // Skip delimiter
                        fields.emplace_back(lineView.substr(fieldStart, fieldEnd - fieldStart));
                        break;
                    }
                    else
                    {
                        // Invalid format - treat as regular character
                        continue;
                    }
                }
            }
            else if (c == m_delimiter && !inQuotes)
            {
                // End of unquoted field
                fields.emplace_back(Trim(std::string(lineView.substr(fieldStart, pos - fieldStart))));
                ++pos; // Skip delimiter
                break;
            }
            else if (c == '\r' && pos + 1 == lineView.length())
            {
                // Skip trailing \r
                break;
            }
            
            ++pos;
        }
        
        // Handle case where we reached end of line
        if (pos >= lineView.length() && fieldStart < lineView.length())
        {
            if (fieldHasQuotes && inQuotes)
            {
                // Unterminated quoted field - include everything from fieldStart
                fields.emplace_back(lineView.substr(fieldStart));
            }
            else
            {
                // Regular field
                fields.emplace_back(Trim(std::string(lineView.substr(fieldStart))));
            }
        }
    }
    
    return fields;
}

db::LogEvent::EventItems CsvParser::CreateEventFromFields(
    const std::vector<std::string>& fields,
    const std::vector<std::string>& headers,
    int eventId)
{
    db::LogEvent::EventItems items;
    
    // Map CSV fields to event items
    for (size_t i = 0; i < fields.size() && i < headers.size(); ++i)
    {
        const std::string& header = headers[i];
        const std::string& value = fields[i];
        
        // Standard field mappings
        if (header == "id")
        {
            items.emplace_back("id", value);
        }
        else if (header == "timestamp" || header == "time" || header == "date")
        {
            items.emplace_back("timestamp", value);
        }
        else if (header == "level" || header == "severity" || header == "priority")
        {
            items.emplace_back("level", value);
        }
        else if (header == "info" || header == "message" || header == "msg" || 
                 header == "description" || header == "text")
        {
            items.emplace_back("info", value);
        }
        else if (header == "source" || header == "logger" || header == "component")
        {
            items.emplace_back("source", value);
        }
        else if (header == "thread" || header == "threadid")
        {
            items.emplace_back("thread", value);
        }
        else if (header == "file" || header == "filename")
        {
            items.emplace_back("file", value);
        }
        else if (header == "line" || header == "lineno")
        {
            items.emplace_back("line", value);
        }
        else if (header == "function" || header == "func")
        {
            items.emplace_back("function", value);
        }
        else
        {
            // Add any other field as-is
            items.emplace_back(header, value);
        }
    }
    
    // Ensure we have at least an ID field
    if (FindHeaderIndex(headers, "id") == -1)
    {
        items.insert(items.begin(), std::make_pair("id", std::to_string(eventId)));
    }
    
    return items;
}

int CsvParser::FindHeaderIndex(const std::vector<std::string>& headers, const std::string& name)
{
    auto it = std::find(headers.begin(), headers.end(), name);
    if (it != headers.end())
    {
        return static_cast<int>(std::distance(headers.begin(), it));
    }
    return -1;
}

std::string CsvParser::Trim(std::string_view str)
{
    if (str.empty())
    {
        return std::string{};
    }
    
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string_view::npos)
    {
        return std::string{};
    }
    
    size_t end = str.find_last_not_of(" \t\r\n");
    return std::string(str.substr(start, end - start + 1));
}

uint32_t CsvParser::GetCurrentProgress() const
{
    return m_currentProgress;
}

uint32_t CsvParser::GetTotalProgress() const
{
    return m_totalProgress;
}

} // namespace parser
