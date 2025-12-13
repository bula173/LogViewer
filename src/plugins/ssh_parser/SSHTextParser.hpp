/**
 * @file SSHTextParser.hpp
 * @brief Text parser for SSH terminal output
 * @author LogViewer Development Team
 * @date 2025
 */

#ifndef SSH_TEXT_PARSER_HPP
#define SSH_TEXT_PARSER_HPP

#include "application/parser/IDataParser.hpp"
#include <regex>
#include <string>
#include <vector>

namespace ssh
{

/**
 * @struct TextLogPattern
 * @brief Pattern definition for parsing text log lines
 */
struct TextLogPattern
{
    std::string name;               ///< Pattern name/identifier
    std::regex pattern;             ///< Regex pattern for matching
    std::vector<std::string> captureGroups; ///< Names of capture groups
    bool enabled = true;            ///< Whether pattern is active
};

/**
 * @class SSHTextParser
 * @brief Parses unstructured text logs from SSH terminal output
 * 
 * This parser handles:
 * - Real-time text log parsing
 * - Regex pattern matching
 * - Multi-line log entry handling
 * - Common log formats (syslog, custom, etc.)
 */
class SSHTextParser : public parser::IDataParser
{
public:
    /**
     * @brief Constructs text parser with default patterns
     */
    SSHTextParser();

    /**
     * @brief Virtual destructor
     */
    virtual ~SSHTextParser() = default;

    /**
     * @brief Parses text data from a file
     * @param filepath Path to text log file
     */
    void ParseData(const std::filesystem::path& filepath) override;

    /**
     * @brief Parses text data from an input stream
     * @param input Input stream containing text logs
     */
    void ParseData(std::istream& input) override;

    /**
     * @brief Gets current parsing progress
     * @return Current bytes processed
     */
    uint32_t GetCurrentProgress() const override;

    /**
     * @brief Gets total expected progress
     * @return Total bytes to process
     */
    uint32_t GetTotalProgress() const override;

    /**
     * @brief Adds a custom log pattern
     * @param pattern Pattern definition
     */
    void AddPattern(const TextLogPattern& pattern);

    /**
     * @brief Sets the default pattern for unmatched lines
     * @param pattern Default pattern to use
     */
    void SetDefaultPattern(const std::string& pattern);

    /**
     * @brief Enables multi-line log entry support
     * @param enabled True to enable multi-line parsing
     */
    void SetMultiLineEnabled(bool enabled);

    /**
     * @brief Parses a single text line
     * @param line Text line to parse
     * @param lineNumber Line number in stream
     * @return Parsed event items or empty if no match
     */
    db::LogEvent::EventItems ParseLine(const std::string& line, int lineNumber);

private:

    /**
     * @brief Tries to match line against all patterns
     * @param line Line to match
     * @return Matching pattern index or -1 if no match
     */
    int MatchPattern(const std::string& line);

    /**
     * @brief Extracts fields from regex match
     * @param match Regex match result
     * @param pattern Pattern that matched
     * @return Extracted event items
     */
    db::LogEvent::EventItems ExtractFields(
        const std::smatch& match,
        const TextLogPattern& pattern);

    /**
     * @brief Initializes default log patterns
     */
    void InitializeDefaultPatterns();

    std::vector<TextLogPattern> m_patterns;  ///< Registered patterns
    std::string m_defaultPattern;            ///< Default fallback pattern
    bool m_multiLineEnabled = false;         ///< Multi-line support
    uint32_t m_currentProgress = 0;          ///< Current progress
    uint32_t m_totalProgress = 0;            ///< Total progress
    std::string m_pendingLine;               ///< Pending multi-line buffer
};

} // namespace ssh

#endif // SSH_TEXT_PARSER_HPP
