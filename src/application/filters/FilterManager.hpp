#pragma once

#include "Filter.hpp"
#include "db/EventsContainer.hpp"
#include <memory>
#include <string>
#include <vector>

namespace filters
{

class FilterManager
{
  public:
    static FilterManager& getInstance();

    // Filter operations
    FilterPtr createFilter(const std::string& name,
        const std::string& columnName, const std::string& pattern,
        bool caseSensitive = false, bool inverted = false);
    void addFilter(const FilterPtr& filter);
    void updateFilter(const FilterPtr& filter);
    void removeFilter(const std::string& filterName);
    void enableFilter(const std::string& filterName, bool enable);
    void enableAllFilters(bool enable);

    // Filter application
    std::vector<size_t> applyFilters(db::EventsContainer& container) const;

    // Filter storage
    void loadFilters();
    void saveFilters();

    // Access filters
    const FilterList& getFilters() const;
    FilterPtr getFilterByName(const std::string& name) const;

    // File operations
    std::string getFiltersFilePath() const;

  private:
    FilterManager();
    ~FilterManager() = default;

    FilterList m_filters;
};

} // namespace filters