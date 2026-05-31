/**
 * @file ParserFactory.cpp
 * @brief Implementation of ParserFactory
 * @author LogViewer Development Team
 * @date 2025
 */

#include "ParserFactory.hpp"
#include "xml/xmlParser.hpp"
#include "csv/CsvParser.hpp"
#include "json/JsonParser.hpp"
#include "asc/AscParser.hpp"
#include "dlt/DltParser.hpp"
#include "evlog/EvlogParser.hpp"
#include "Logger.hpp"
#include <algorithm>

namespace parser {

// Static member initialization
std::map<std::string, ParserFactory::CreatorFunc> ParserFactory::s_creators;

void ParserFactory::InitializeDefaults()
{
    util::Logger::Debug("ParserFactory::InitializeDefaults - Registering default parsers");

    // Register XML parser
    Register(".xml", []() {
        util::Logger::Debug("Creating XmlParser instance");
        return std::make_unique<XmlParser>();
    });

    // Register CSV parser
    Register(".csv", []() {
        util::Logger::Debug("Creating CsvParser instance");
        return std::make_unique<CsvParser>();
    });

    // Register JSON parser (array and NDJSON/JSONL formats).
    Register(".json", []() {
        util::Logger::Debug("Creating JsonParser instance");
        return std::make_unique<JsonParser>();
    });
    Register(".jsonl", []() {
        util::Logger::Debug("Creating JsonParser instance (JSONL)");
        return std::make_unique<JsonParser>();
    });

    // Register ASC (Vector CANalyzer) parser — no DBC by default; callers that
    // need DBC decoding should create AscParser directly and pass it to the
    // presenter's LoadLogFile(parser) overload.
    Register(".asc", []() {
        util::Logger::Debug("Creating AscParser instance");
        return std::make_unique<AscParser>();
    });

    // Register DLT (AUTOSAR Diagnostic Log and Trace) parser.
    Register(".dlt", []() {
        util::Logger::Debug("Creating DltParser instance");
        return std::make_unique<DltParser>();
    });

    // Register Evlog (POSIX 1003.25 Enterprise Event Logging) parser.
    Register(".evl", []() {
        util::Logger::Debug("Creating EvlogParser instance");
        return std::make_unique<EvlogParser>();
    });
}

void ParserFactory::EnsureInitialized()
{
    if (s_creators.empty()) {
        InitializeDefaults();
    }
}

util::Result<std::unique_ptr<IDataParser>, error::Error> ParserFactory::CreateFromFile(
    const std::filesystem::path& filepath)
{
    if (filepath.empty()) {
        util::Logger::Error("ParserFactory::CreateFromFile - Empty filepath provided");
        return util::Result<std::unique_ptr<IDataParser>, error::Error>::Err(
            error::Error(error::ErrorCode::InvalidArgument, "Filepath cannot be empty"));
    }

    EnsureInitialized();

    std::string extension = filepath.extension().string();
    std::transform(extension.begin(), extension.end(), extension.begin(),
        [](unsigned char c) { return std::tolower(c); });

    util::Logger::Info("ParserFactory::CreateFromFile - Creating parser for extension: {}", 
        extension);

    auto it = s_creators.find(extension);
    if (it != s_creators.end()) {
        try {
            auto parser = it->second();
            util::Logger::Info("ParserFactory::CreateFromFile - Successfully created parser");
            return util::Result<std::unique_ptr<IDataParser>, error::Error>::Ok(std::move(parser));
        } catch (const std::exception& e) {
            util::Logger::Error("ParserFactory::CreateFromFile - Exception creating parser: {}", e.what());
            return util::Result<std::unique_ptr<IDataParser>, error::Error>::Err(
                error::Error(error::ErrorCode::RuntimeError, 
                    std::string("Failed to create parser: ") + e.what()));
        }
    }

    // Fallback to XML parser for unknown extensions
    util::Logger::Warn("ParserFactory::CreateFromFile - Unknown extension '{}', defaulting to XML parser",
        extension);
    return util::Result<std::unique_ptr<IDataParser>, error::Error>::Ok(
        std::make_unique<XmlParser>());
}

util::Result<std::unique_ptr<IDataParser>, error::Error> ParserFactory::Create(ParserType type)
{
    EnsureInitialized();

    switch (type) {
    case ParserType::XML:
        util::Logger::Debug("ParserFactory::Create - Creating XML parser");
        return util::Result<std::unique_ptr<IDataParser>, error::Error>::Ok(
            std::make_unique<XmlParser>());

    case ParserType::JSON:
        util::Logger::Debug("ParserFactory::Create - Creating JSON parser");
        return util::Result<std::unique_ptr<IDataParser>, error::Error>::Ok(
            std::make_unique<JsonParser>());

    case ParserType::CSV:
        util::Logger::Debug("ParserFactory::Create - Creating CSV parser");
        return util::Result<std::unique_ptr<IDataParser>, error::Error>::Ok(
            std::make_unique<CsvParser>());

    case ParserType::Custom:
        util::Logger::Error("ParserFactory::Create - Custom parser must be accessed via extension");
        return util::Result<std::unique_ptr<IDataParser>, error::Error>::Err(
            error::Error(error::ErrorCode::InvalidArgument, 
                "Custom parsers must be created via CreateFromFile"));

    default:
        util::Logger::Error("ParserFactory::Create - Unknown parser type: {}",
            static_cast<int>(type));
        return util::Result<std::unique_ptr<IDataParser>, error::Error>::Err(
            error::Error(error::ErrorCode::InvalidArgument, "Unknown parser type"));
    }
}

std::optional<error::Error> ParserFactory::Register(const std::string& extension, CreatorFunc creator)
{
    if (extension.empty()) {
        util::Logger::Error("ParserFactory::Register - Empty extension provided");
        return error::Error(error::ErrorCode::InvalidArgument, "Extension cannot be empty");
    }

    if (!creator) {
        util::Logger::Error("ParserFactory::Register - Null creator function provided");
        return error::Error(error::ErrorCode::InvalidArgument, "Creator function cannot be null");
    }

    std::string lowerExtension = extension;
    std::transform(lowerExtension.begin(), lowerExtension.end(),
        lowerExtension.begin(),
        [](unsigned char c) { return std::tolower(c); });

    // Ensure extension starts with '.'
    if (lowerExtension[0] != '.') {
        lowerExtension = "." + lowerExtension;
    }

    util::Logger::Info("ParserFactory::Register - Registering parser for extension: {}",
        lowerExtension);

    s_creators[lowerExtension] = std::move(creator);
    
    return std::nullopt; // Success
}

bool ParserFactory::IsRegistered(const std::string& extension)
{
    EnsureInitialized();

    std::string lowerExtension = extension;
    std::transform(lowerExtension.begin(), lowerExtension.end(),
        lowerExtension.begin(),
        [](unsigned char c) { return std::tolower(c); });

    return s_creators.find(lowerExtension) != s_creators.end();
}

std::vector<std::string> ParserFactory::GetSupportedExtensions()
{
    EnsureInitialized();

    std::vector<std::string> extensions;
    extensions.reserve(s_creators.size());

    for (const auto& [ext, _] : s_creators) {
        extensions.push_back(ext);
    }

    util::Logger::Debug("ParserFactory::GetSupportedExtensions - Returning {} extensions",
        extensions.size());

    return extensions;
}

} // namespace parser
