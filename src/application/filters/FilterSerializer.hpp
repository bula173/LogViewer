#pragma once

#include "Filter.hpp"
#include <vector>
#include <string>

namespace filters
{

/**
 * @brief Serializes and deserializes filter configurations.
 *
 * Enables saving filter sets as JSON files for team sharing and
 * reusability across sessions.
 *
 * File Format: filters.json (v1.0)
 * ```json
 * {
 *   "version": "1.0",
 *   "timestamp": "2026-07-30T10:00:00Z",
 *   "filters": [
 *     {"name": "ERROR", "target": "level", "pattern": "ERROR", "enabled": true}
 *   ]
 * }
 * ```
 */
class FilterSerializer
{
  public:
    /**
     * @brief Export filter set to JSON file.
     * @param filters Vector of filters to export
     * @param filename Path to save file
     * @return Error string (empty if success)
     */
    static std::string ExportFilters(
        const std::vector<Filter>& filters,
        const std::string& filename);

    /**
     * @brief Import filter set from JSON file.
     * @param filename Path to load file
     * @param outFilters Vector to populate with imported filters
     * @return Error string (empty if success)
     */
    static std::string ImportFilters(
        const std::string& filename,
        std::vector<Filter>& outFilters);
};

} // namespace filters
