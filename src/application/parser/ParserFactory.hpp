/**
 * @file ParserFactory.hpp
 * @brief Factory for creating parser instances based on file type
 * @author LogViewer Development Team
 * @date 2025
 *
 * Implements the Factory design pattern for parser instantiation.
 * Supports runtime registration of custom parsers and automatic
 * parser selection based on file extension.
 *
 * @par Example Usage:
 * @code
 * // Automatic parser selection
 * auto parser = ParserFactory::CreateFromFile("logfile.xml");
 * parser->RegisterObserver(observer);
 * parser->ParseData("logfile.xml");
 *
 * // Explicit parser type
 * auto xmlParser = ParserFactory::Create(ParserType::XML);
 *
 * // Register custom parser
 * ParserFactory::Register(".custom", []() {
 *     return std::make_unique<CustomParser>();
 * });
 * @endcode
 */

#pragma once

#include "parser/IDataParser.hpp"
#include "util/Result.hpp"
#include "error/Error.hpp"
#include <filesystem>
#include <functional>
#include <map>
#include <memory>
#include <optional>
#include <string>

namespace parser {

/**
 * @enum ParserType
 * @brief Supported parser types
 */
enum class ParserType {
    XML,    ///< XML log file parser
    JSON,   ///< JSON log file parser (future)
    CSV,    ///< CSV log file parser (future)
    Custom  ///< Custom registered parser
};

/**
 * @class ParserFactory
 * @brief Factory class for creating parser instances
 *
 * Provides centralized parser creation with support for:
 * - Automatic parser selection based on file extension
 * - Explicit parser type specification
 * - Runtime registration of custom parsers
 * - Extension to parser type mapping
 *
 * @par Thread Safety
 * The factory uses a static map for parser creators. Registration
 * should be done during application initialization before concurrent
 * access begins.
 *
 * @par Design Pattern
 * Implements the Factory Method pattern with additional registry
 * functionality for extensibility.
 */
class ParserFactory {
public:
    /// Type alias for parser creator function
    using CreatorFunc = std::function<std::unique_ptr<IDataParser>()>;

    /**
     * @brief Creates a parser based on file extension
     *
     * Examines the file extension and returns an appropriate parser.
     * Falls back to XML parser if extension is not recognized.
     *
     * @param filepath Path to the file to parse
     * @return Result with parser instance or error
     *
     * @par Example:
     * @code
     * auto result = ParserFactory::CreateFromFile("log.xml");
     * if (result.isOk()) {
     *     auto parser = result.unwrap();
     *     parser->ParseData("log.xml");
     * } else {
     *     // Handle error
     *     std::cerr << result.unwrapErr().message() << std::endl;
     * }
     * @endcode
     */
    static util::Result<std::unique_ptr<IDataParser>, error::Error> CreateFromFile(
        const std::filesystem::path& filepath);

    /**
     * @brief Creates a parser of specific type
     *
     * @param type The type of parser to create
     * @return Result with parser instance or error
     *
     * @par Example:
     * @code
     * auto result = ParserFactory::Create(ParserType::XML);
     * if (result.isOk()) {
     *     auto parser = result.unwrap();
     * }
     * @endcode
     */
    static util::Result<std::unique_ptr<IDataParser>, error::Error> Create(ParserType type);

    /**
     * @brief Registers a custom parser creator for a file extension
     *
     * Allows runtime registration of parsers for custom file formats.
     * The creator function will be called whenever a file with the
     * specified extension needs parsing.
     *
     * @param extension File extension (e.g., ".log", ".txt")
     * @param creator Function that creates parser instances
     * @return Optional error if registration failed, std::nullopt on success
     *
     * @par Example:
     * @code
     * auto error = ParserFactory::Register(".mylog", []() {
     *     return std::make_unique<MyLogParser>();
     * });
     * if (error) {
     *     // Handle registration error
     *     util::Logger::Error("Registration failed: {}", error->message());
     * }
     * @endcode
     */
    static std::optional<error::Error> Register(const std::string& extension, CreatorFunc creator);

    /**
     * @brief Checks if a parser is registered for an extension
     *
     * @param extension File extension to check
     * @return true if parser is registered, false otherwise
     */
    static bool IsRegistered(const std::string& extension);

    /**
     * @brief Gets all registered file extensions
     *
     * @return Vector of registered extensions
     *
     * @par Example:
     * @code
     * auto extensions = ParserFactory::GetSupportedExtensions();
     * // Use in file dialog filter
     * @endcode
     */
    static std::vector<std::string> GetSupportedExtensions();

private:
    /// Map of file extensions to parser creators
    static std::map<std::string, CreatorFunc> s_creators;

    /// Initializes default parsers (XML, JSON, etc.)
    static void InitializeDefaults();

    /// Ensures default parsers are registered (called automatically)
    static void EnsureInitialized();
};

} // namespace parser
