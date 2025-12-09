/**
 * @file CsvParser.hpp
 * @brief CSV log file parser implementation
 * @author LogViewer Development Team
 * @date 2025
 */

#ifndef PARSER_CSVPARSER_HPP
#define PARSER_CSVPARSER_HPP

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <istream>
#include <string>
#include <string_view>
#include <vector>

#include "parser/IDataParser.hpp"

namespace parser
{

/**
 * @class CsvParser
 * @brief Parses CSV formatted log files
 *
 * This parser handles CSV (Comma-Separated Values) log files with the following format:
 * - First line contains column headers (e.g., id, timestamp, level, info)
 * - Subsequent lines contain log event data
 * - Fields can be quoted to handle commas within values
 * - Supports various delimiters (comma, semicolon, tab)
 *
 * @par Expected CSV Format:
 * @code
 * id,timestamp,level,info
 * 1,2025-12-07 10:30:15,INFO,Application started
 * 2,2025-12-07 10:30:16,DEBUG,Initializing modules
 * @endcode
 *
 * @par Thread Safety
 * This class is not thread-safe. Each parsing operation should use a separate
 * instance or external synchronization.
 */
class CsvParser : public IDataParser
{
  public:
    /**
     * @brief Constructs a new CSV parser with default settings
     */
    CsvParser();
    
    /**
     * @brief Virtual destructor for proper cleanup
     */
    virtual ~CsvParser() = default;

    /**
     * @brief Parses CSV data from a file
     *
     * @param filepath Path to the CSV file to parse
     * @throws error::Error if the file cannot be opened or parsed
     */
    void ParseData(const std::filesystem::path& filepath) override;

    /**
     * @brief Parses CSV data from an input stream
     *
     * @param input The input stream containing CSV data
     * @throws error::Error if the stream cannot be read or parsed
     */
    void ParseData(std::istream& input) override;

    /**
     * @brief Gets the current parsing progress
     *
     * @return Current number of bytes processed
     */
    uint32_t GetCurrentProgress() const override;

    /**
     * @brief Gets the total expected progress
     *
     * @return Total number of bytes to process (0 if unknown)
     */
    uint32_t GetTotalProgress() const override;

    /**
     * @brief Sets the field delimiter character
     *
     * @param delimiter Character to use as field separator (default: ',')
     */
    void SetDelimiter(char delimiter) { m_delimiter = delimiter; }

    /**
     * @brief Sets whether the first line contains headers
     *
     * @param hasHeaders True if first line contains column names
     */
    void SetHasHeaders(bool hasHeaders) { m_hasHeaders = hasHeaders; }

  private:
    /**
     * @brief Parses a single CSV line into fields
     *
     * @param line The CSV line to parse
     * @return Vector of field values
     */
    std::vector<std::string> ParseLine(const std::string& line);

    /**
     * @brief Creates EventItems from parsed CSV fields
     *
     * @param fields Vector of field values
     * @param headers Vector of column headers
     * @param eventId Sequential event ID
     * @return EventItems containing the parsed fields
     */
    db::LogEvent::EventItems CreateEventFromFields(
        const std::vector<std::string>& fields,
        const std::vector<std::string>& headers,
        int eventId);

    /**
     * @brief Finds the index of a header by name
     *
     * @param headers Vector of header names
     * @param name Header name to find
     * @return Index of the header, or -1 if not found
     */
    int FindHeaderIndex(const std::vector<std::string>& headers, const std::string& name);

    /**
     * @brief Trims whitespace from both ends of a string
     *
     * @param str String to trim
     * @return Trimmed string
     */
    std::string Trim(std::string_view str);

    char m_delimiter = ',';        ///< Field delimiter character
    bool m_hasHeaders = true;      ///< Whether first line contains headers
    uint32_t m_currentProgress = 0; ///< Current bytes processed
    uint32_t m_totalProgress = 0;   ///< Total bytes to process
};

} // namespace parser

#endif // PARSER_CSVPARSER_HPP
