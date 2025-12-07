/**
 * @file CsvParser.cpp
 * @brief Implementation of CSV log file parser
 * @author LogViewer Development Team
 * @date 2025
 */

#include "csv/CsvParser.hpp"
#include "db/LogEvent.hpp"
#include "error/Error.hpp"
#include "util/Logger.hpp"

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
    constexpr size_t BATCH_SIZE = 100;
    std::vector<std::pair<int, db::LogEvent::EventItems>> eventBatch;
    eventBatch.reserve(BATCH_SIZE);

    // Read and parse each line
    while (std::getline(input, line))
    {
        lineNumber++;
        m_currentProgress = static_cast<uint32_t>(input.tellg());

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
                NotifyNewEventBatch(std::move(eventBatch));
                eventBatch.clear();
                eventBatch.reserve(BATCH_SIZE);
                
                // Notify progress periodically
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
    std::string field;
    bool inQuotes = false;
    
    for (size_t i = 0; i < line.length(); ++i)
    {
        char c = line[i];
        
        // Handle quotes
        if (c == '"')
        {
            if (inQuotes && i + 1 < line.length() && line[i + 1] == '"')
            {
                // Escaped quote within quoted field
                field += '"';
                ++i; // Skip next quote
            }
            else
            {
                // Toggle quote state
                inQuotes = !inQuotes;
            }
        }
        // Handle delimiter
        else if (c == m_delimiter && !inQuotes)
        {
            fields.push_back(Trim(field));
            field.clear();
        }
        // Regular character
        else
        {
            // Skip \r at end of line (for Windows line endings)
            if (c == '\r' && i + 1 == line.length())
            {
                continue;
            }
            field += c;
        }
    }
    
    // Add the last field
    fields.push_back(Trim(field));
    
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

std::string CsvParser::Trim(const std::string& str)
{
    if (str.empty())
    {
        return str;
    }
    
    size_t start = str.find_first_not_of(" \t\r\n");
    if (start == std::string::npos)
    {
        return "";
    }
    
    size_t end = str.find_last_not_of(" \t\r\n");
    return str.substr(start, end - start + 1);
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
