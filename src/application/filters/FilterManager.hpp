#pragma once

#include "Filter.hpp"
#include "IModel.hpp"
#include "Error.hpp"
#include "Result.hpp"

#include <memory>
#include <string>
#include <variant>
#include <vector>

namespace filters
{

using FilterResult = util::Result<std::monostate, error::Error>;

/**
 * @class FilterManager
 * @brief Owns all user-defined filters, handles persistence, and provides
 *        helpers to apply filters against the events container.
 */
class FilterManager
{
  public:
    static FilterManager& getInstance();

    // Filter operations
    /**
     * @brief Create a new filter instance with the provided parameters.
     */
    FilterPtr createFilter(const std::string& name,
        const std::string& columnName, const std::string& pattern,
        bool caseSensitive = false, bool inverted = false);
    /**
     * @brief Add a filter to the managed list. Duplicate names are rejected.
     */
    void addFilter(const FilterPtr& filter);
    /**
     * @brief Replace an existing filter (matched by name) or append it.
     */
    void updateFilter(const FilterPtr& filter);
    /**
     * @brief Remove a filter identified by name.
     */
    void removeFilter(const std::string& filterName);
    /**
     * @brief Enable/disable a single filter.
     */
    void enableFilter(const std::string& filterName, bool enable);
    /**
     * @brief Toggle all known filters at once.
     */
    void enableAllFilters(bool enable);

    // Filter application
    /**
     * @brief Apply all enabled filters and return the matching indices.
     */
    std::vector<unsigned long> applyFilters(
      const mvc::IModel& model) const;

    // Filter storage
    /**
     * @brief Load filters from the default filters.json path.
     */
    FilterResult loadFilters();
    /**
     * @brief Persist filters to the default filters.json path.
     */
    [[nodiscard]] FilterResult saveFilters() const;

    // Access filters
    const FilterList& getFilters() const;
    FilterPtr getFilterByName(const std::string& name) const;

    // File operations
    std::string getFiltersFilePath() const;

    // Custom path methods
    [[nodiscard]] FilterResult saveFiltersToPath(const std::string& path) const;
    FilterResult loadFiltersFromPath(const std::string& path);

  private:
    FilterManager();
    ~FilterManager() = default;

    FilterList m_filters;
};

} // namespace filters