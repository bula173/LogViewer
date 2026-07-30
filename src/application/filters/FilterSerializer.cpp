#include "FilterSerializer.hpp"
#include "Logger.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <chrono>
#include <iomanip>

using json = nlohmann::json;

namespace filters
{

std::string FilterSerializer::ExportFilters(
    const std::vector<Filter>& filters,
    const std::string& filename)
{
    try
    {
        // Create filter array
        json filterArray = json::array();
        for (const auto& filter : filters)
        {
            json filterObj = {
                {"name", filter.name},
                {"target", filter.columnName},
                {"pattern", filter.pattern},
                {"enabled", filter.isEnabled},
                {"inverted", filter.isInverted},
                {"caseSensitive", filter.isCaseSensitive}
            };
            filterArray.push_back(filterObj);
        }

        // Create export document
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::gmtime(&time), "%Y-%m-%dT%H:%M:%SZ");

        json export_doc = {
            {"version", "1.0"},
            {"timestamp", ss.str()},
            {"count", filters.size()},
            {"filters", filterArray}
        };

        // Write to file
        std::ofstream outFile(filename);
        if (!outFile.is_open())
        {
            return "Failed to open file for writing: " + filename;
        }

        outFile << export_doc.dump(2);
        outFile.close();

        util::Logger::Info("[FilterSerializer] Exported {} filters to '{}'",
            filters.size(), filename);
        return "";  // Success
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[FilterSerializer] Export failed: {}", e.what());
        return std::string("Export failed: ") + e.what();
    }
}

std::string FilterSerializer::ImportFilters(
    const std::string& filename,
    std::vector<Filter>& outFilters)
{
    try
    {
        std::ifstream inFile(filename);
        if (!inFile.is_open())
        {
            return "Failed to open file for reading: " + filename;
        }

        json import_doc = json::parse(inFile);
        inFile.close();

        // Validate version
        if (!import_doc.contains("version"))
        {
            return "Invalid format: missing version field";
        }

        const std::string version = import_doc["version"];
        if (version != "1.0")
        {
            return "Unsupported format version: " + version;
        }

        // Import filters
        outFilters.clear();
        if (!import_doc.contains("filters"))
        {
            return "Invalid format: missing filters array";
        }

        const auto& filterArray = import_doc["filters"];
        if (!filterArray.is_array())
        {
            return "Invalid format: filters is not an array";
        }

        for (const auto& filterObj : filterArray)
        {
            if (!filterObj.contains("name") || !filterObj.contains("target") ||
                !filterObj.contains("pattern"))
            {
                return "Invalid filter object: missing required fields";
            }

            // Reconstruct filter from JSON
            Filter importedFilter(
                filterObj["name"].get<std::string>(),
                filterObj["target"].get<std::string>(),
                filterObj["pattern"].get<std::string>());

            // Restore optional properties
            if (filterObj.contains("enabled"))
            {
                importedFilter.isEnabled = filterObj["enabled"].get<bool>();
            }
            if (filterObj.contains("inverted"))
            {
                importedFilter.isInverted = filterObj["inverted"].get<bool>();
            }
            if (filterObj.contains("caseSensitive"))
            {
                importedFilter.isCaseSensitive = filterObj["caseSensitive"].get<bool>();
            }

            outFilters.push_back(importedFilter);
        }

        util::Logger::Info("[FilterSerializer] Imported {} filters from '{}'",
            outFilters.size(), filename);
        return "";  // Success
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[FilterSerializer] Import failed: {}", e.what());
        return std::string("Import failed: ") + e.what();
    }
}

} // namespace filters
