/**
 * @file SSHTextParser.cpp
 * @brief Implementation of SSH text parser
 * @author LogViewer Development Team
 * @date 2025
 */

#include "SSHTextParser.hpp"
#include "db/LogEvent.hpp"
#include "error/Error.hpp"
#include "util/Logger.hpp"
#include <fstream>
#include <sstream>

namespace ssh
{

SSHTextParser::SSHTextParser()
    : parser::IDataParser()
{
    InitializeDefaultPatterns();
    util::Logger::Debug("SSHTextParser constructor called");
}

void SSHTextParser::ParseData(const std::filesystem::path& filepath)
{
    util::Logger::Debug("SSHTextParser::ParseData called with filepath: {}", filepath.string());

    std::ifstream input(filepath);
    if (!input.is_open())
    {
        throw error::Error(
            "SSHTextParser::ParseData failed to open file: " + filepath.string());
    }

    // Determine file size for progress tracking
    input.seekg(0, std::ios::end);
    m_totalProgress = static_cast<uint32_t>(input.tellg());
    input.seekg(0, std::ios::beg);
    m_currentProgress = 0;

    ParseData(input);
    input.close();
}

void SSHTextParser::ParseData(std::istream& input)
{
    util::Logger::Debug("SSHTextParser::ParseData called with istream");

    if (!input.good())
    {
        throw error::Error(
            "SSHTextParser::ParseData: input stream is not readable");
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

    std::string line;
    int eventId = 0;
    size_t lineNumber = 0;
    constexpr size_t BATCH_SIZE = 100;
    std::vector<std::pair<int, db::LogEvent::EventItems>> eventBatch;
    eventBatch.reserve(BATCH_SIZE);

    while (std::getline(input, line))
    {
        lineNumber++;
        m_currentProgress = static_cast<uint32_t>(input.tellg());

        // Skip empty lines
        if (line.empty())
        {
            continue;
        }

        try
        {
            db::LogEvent::EventItems items = ParseLine(line, lineNumber);
            
            if (!items.empty())
            {
                eventBatch.emplace_back(++eventId, std::move(items));

                // Send batch when full
                if (eventBatch.size() >= BATCH_SIZE)
                {
                    NotifyNewEventBatch(std::move(eventBatch));
                    eventBatch.clear();
                    eventBatch.reserve(BATCH_SIZE);
                    NotifyProgressUpdated();
                }
            }
        }
        catch (const std::exception& e)
        {
            util::Logger::Warn("SSHTextParser: Failed to parse line {}: {}",
                lineNumber, e.what());
        }
    }

    // Send remaining events
    if (!eventBatch.empty())
    {
        NotifyNewEventBatch(std::move(eventBatch));
    }

    m_currentProgress = m_totalProgress;
    NotifyProgressUpdated();

    util::Logger::Info("SSHTextParser: Parsed {} events from {} lines",
        eventId, lineNumber);
}

db::LogEvent::EventItems SSHTextParser::ParseLine(const std::string& line, int lineNumber)
{
    db::LogEvent::EventItems items;

    // Try to match against registered patterns
    int patternIndex = MatchPattern(line);
    
    if (patternIndex >= 0)
    {
        // Match found - extract fields
        const auto& pattern = m_patterns[patternIndex];
        std::smatch match;
        
        if (std::regex_search(line, match, pattern.pattern))
        {
            items = ExtractFields(match, pattern);
        }
    }
    else
    {
        // No pattern matched - create generic entry
        items.emplace_back("id", std::to_string(lineNumber));
        items.emplace_back("line", std::to_string(lineNumber));
        items.emplace_back("info", line);
        items.emplace_back("level", "INFO");
    }

    return items;
}

int SSHTextParser::MatchPattern(const std::string& line)
{
    for (size_t i = 0; i < m_patterns.size(); ++i)
    {
        if (!m_patterns[i].enabled)
        {
            continue;
        }

        if (std::regex_search(line, m_patterns[i].pattern))
        {
            return static_cast<int>(i);
        }
    }

    return -1;
}

db::LogEvent::EventItems SSHTextParser::ExtractFields(
    const std::smatch& match,
    const TextLogPattern& pattern)
{
    db::LogEvent::EventItems items;

    // Extract captured groups based on pattern definition
    for (size_t i = 0; i < pattern.captureGroups.size() && i + 1 < match.size(); ++i)
    {
        const std::string& fieldName = pattern.captureGroups[i];
        const std::string& fieldValue = match[i + 1].str();
        
        items.emplace_back(fieldName, fieldValue);
    }

    return items;
}

void SSHTextParser::InitializeDefaultPatterns()
{
    // Standard syslog pattern
    // Format: Jan  1 10:30:15 hostname program[pid]: message
    TextLogPattern syslogPattern;
    syslogPattern.name = "syslog";
    syslogPattern.pattern = std::regex(
        R"(^(\w+\s+\d+\s+\d+:\d+:\d+)\s+(\S+)\s+([^\[]+)\[(\d+)\]:\s+(.*)$)");
    syslogPattern.captureGroups = {"timestamp", "hostname", "program", "pid", "info"};
    m_patterns.push_back(syslogPattern);

    // Timestamped log pattern
    // Format: [2025-12-07 10:30:15] [LEVEL] message
    TextLogPattern timestampPattern;
    timestampPattern.name = "timestamp_level";
    timestampPattern.pattern = std::regex(
        R"(^\[([^\]]+)\]\s*\[([^\]]+)\]\s+(.*)$)");
    timestampPattern.captureGroups = {"timestamp", "level", "info"};
    m_patterns.push_back(timestampPattern);

    // Simple timestamped pattern
    // Format: 2025-12-07 10:30:15 message
    TextLogPattern simplePattern;
    simplePattern.name = "simple_timestamp";
    simplePattern.pattern = std::regex(
        R"(^(\d{4}-\d{2}-\d{2}\s+\d{2}:\d{2}:\d{2})\s+(.*)$)");
    simplePattern.captureGroups = {"timestamp", "info"};
    m_patterns.push_back(simplePattern);

    // Level-prefixed pattern
    // Format: INFO: message or ERROR: message
    TextLogPattern levelPattern;
    levelPattern.name = "level_prefix";
    levelPattern.pattern = std::regex(
        R"(^(ERROR|WARN|WARNING|INFO|DEBUG|TRACE):\s*(.*)$)", std::regex::icase);
    levelPattern.captureGroups = {"level", "info"};
    m_patterns.push_back(levelPattern);

    util::Logger::Debug("Initialized {} default patterns", m_patterns.size());
}

void SSHTextParser::AddPattern(const TextLogPattern& pattern)
{
    m_patterns.push_back(pattern);
    util::Logger::Debug("Added custom pattern: {}", pattern.name);
}

void SSHTextParser::SetDefaultPattern(const std::string& pattern)
{
    m_defaultPattern = pattern;
}

void SSHTextParser::SetMultiLineEnabled(bool enabled)
{
    m_multiLineEnabled = enabled;
}

uint32_t SSHTextParser::GetCurrentProgress() const
{
    return m_currentProgress;
}

uint32_t SSHTextParser::GetTotalProgress() const
{
    return m_totalProgress;
}

} // namespace ssh
