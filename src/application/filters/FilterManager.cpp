#include "FilterManager.hpp"

#include "Config.hpp"
#include "Logger.hpp"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <unordered_set>
#include <nlohmann/json.hpp>

namespace filters
{

FilterManager& FilterManager::getInstance()
{
    static FilterManager instance;
    return instance;
}

FilterManager::FilterManager()
{
    auto result = loadFilters();
    if (result.isErr())
    {
        const auto& err = result.error();
        if (err.code() == error::ErrorCode::FileNotFound)
        {
            util::Logger::Info(
                "Filters file not found yet: {}", err.what());
        }
        else
        {
            util::Logger::Warn("Initial filter load failed: {}", err.what());
        }
    }
}

FilterPtr FilterManager::createFilter(const std::string& name,
    const std::string& columnName, const std::string& pattern,
    bool caseSensitive, bool inverted)
{
    return std::make_shared<Filter>(
        name, columnName, pattern, caseSensitive, inverted);
}

void FilterManager::addFilter(const FilterPtr& filter)
{
    if (!filter)
        return;

    // Check for duplicate name
    for (const auto& existingFilter : m_filters)
    {
        if (existingFilter->name == filter->name)
        {
            util::Logger::Error(
                "Filter with name '{}' already exists", filter->name);
            return;
        }
    }

    m_filters.push_back(filter);
    util::Logger::Debug("Added filter: {}", filter->name);
}

void FilterManager::updateFilter(const FilterPtr& filter)
{
    if (!filter)
        return;

    for (auto& existingFilter : m_filters)
    {
        if (existingFilter->name == filter->name)
        {
            existingFilter = filter;
            util::Logger::Debug("Updated filter: {}", filter->name);
            return;
        }
    }

    // If not found, add as new
    m_filters.push_back(filter);
    util::Logger::Debug("Added filter (during update): {}", filter->name);
}

void FilterManager::removeFilter(const std::string& filterName)
{
    auto it = std::find_if(m_filters.begin(), m_filters.end(),
        [&](const FilterPtr& f) { return f->name == filterName; });

    if (it != m_filters.end())
    {
        m_filters.erase(it);
        util::Logger::Debug("Removed filter: {}", filterName);
    }
}

void FilterManager::enableFilter(const std::string& filterName, bool enable)
{
    for (auto& filter : m_filters)
    {
        if (filter->name == filterName)
        {
            filter->isEnabled = enable;
            util::Logger::Debug("{} filter: {}",
                enable ? "Enabled" : "Disabled", filterName);
            return;
        }
    }
}

void FilterManager::enableAllFilters(bool enable)
{
    for (auto& filter : m_filters)
    {
        filter->isEnabled = enable;
    }
    util::Logger::Debug("{} all filters", enable ? "Enabled" : "Disabled");
}

std::vector<unsigned long> FilterManager::applyFilters(
    const mvc::IModel& model) const
{
    // If no filters enabled, include all events
    size_t enabledFilters = 0;
    for (const auto& filter : m_filters)
    {
        if (filter && filter->isEnabled)
            enabledFilters++;
    }

    if (enabledFilters == 0)
    {
        std::vector<unsigned long> result(model.Size());
        for (unsigned long i = 0; i < model.Size(); ++i)
        {
            result[i] = i;
        }
        return result;
    }

    // Create list of all indices
    std::vector<unsigned long> allIndices(model.Size());
    for (unsigned long i = 0; i < model.Size(); ++i)
    {
        allIndices[i] = i;
    }

    // Apply each filter and collect matching indices (OR logic between filters)
    std::vector<unsigned long> result;
    std::unordered_set<unsigned long> seen; // To avoid duplicates

    for (const auto& filter : m_filters)
    {
        if (!filter || !filter->isEnabled)
            continue;

        auto filteredIndices = filter->applyToIndices(allIndices, model);
        for (unsigned long idx : filteredIndices)
        {
            if (seen.find(idx) == seen.end())
            {
                seen.insert(idx);
                result.push_back(idx);
            }
        }
    }

    // Sort indices to maintain original event order
    std::sort(result.begin(), result.end());

    return result;
}

FilterResult FilterManager::loadFilters()
{
    return loadFiltersFromPath(getFiltersFilePath());
}

FilterResult FilterManager::saveFilters() const
{
    return saveFiltersToPath(getFiltersFilePath());
}

FilterResult FilterManager::saveFiltersToPath(const std::string& path) const
{
    try
    {
        if (path.empty())
        {
            return FilterResult::Err(error::Error(
                error::ErrorCode::InvalidArgument,
                "Filters file path is empty", false));
        }

        std::filesystem::path filePath(path);
        if (!filePath.parent_path().empty())
        {
            std::filesystem::create_directories(filePath.parent_path());
        }

        nlohmann::json jsonFilters = nlohmann::json::array();
        for (const auto& filter : m_filters)
        {
            if (filter)
                jsonFilters.push_back(filter->toJson());
        }

        std::ofstream file(path);
        if (!file.is_open())
        {
            return FilterResult::Err(error::Error(error::ErrorCode::IOError,
                "Failed to open filters file for writing: " + path, false));
        }

        file << jsonFilters.dump(2);
        util::Logger::Info(
            "Saved {} filters to {}", m_filters.size(), path);
        return FilterResult::Ok(std::monostate {});
    }
    catch (const std::exception& e)
    {
        return FilterResult::Err(error::Error(error::ErrorCode::IOError,
            std::string("Error saving filters to ") + path + ": " + e.what(),
            false));
    }
}

FilterResult FilterManager::loadFiltersFromPath(const std::string& path)
{
    if (path.empty())
    {
        return FilterResult::Err(error::Error(error::ErrorCode::InvalidArgument,
            "Filters file path is empty", false));
    }

    if (!std::filesystem::exists(path))
    {
        return FilterResult::Err(error::Error(error::ErrorCode::FileNotFound,
            "Filters file does not exist at: " + path, false));
    }

    try
    {
        std::ifstream file(path);
        if (!file.is_open())
        {
            return FilterResult::Err(error::Error(error::ErrorCode::IOError,
                "Failed to open filters file: " + path, false));
        }

        nlohmann::json jsonFilters;
        file >> jsonFilters;

        if (!jsonFilters.is_array())
        {
            return FilterResult::Err(error::Error(error::ErrorCode::ParseError,
                "Filters file must contain an array: " + path, false));
        }

        FilterList newFilters;
        for (const auto& filterJson : jsonFilters)
        {
            try
            {
                Filter filter = Filter::fromJson(filterJson);
                newFilters.push_back(std::make_shared<Filter>(filter));
            }
            catch (const std::exception& e)
            {
                util::Logger::Error("Error parsing filter entry: {}", e.what());
            }
        }

        m_filters = std::move(newFilters);
        util::Logger::Info(
            "Loaded {} filters from {}", m_filters.size(), path);
        return FilterResult::Ok(std::monostate {});
    }
    catch (const std::exception& e)
    {
        return FilterResult::Err(error::Error(error::ErrorCode::IOError,
            std::string("Error loading filters from ") + path + ": " +
                e.what(),
            false));
    }
}

std::string FilterManager::getFiltersFilePath() const
{
    // Get config path and use same directory for filters
    std::filesystem::path configPath = config::GetConfig().GetConfigFilePath();
    std::filesystem::path filtersPath =
        configPath.parent_path() / "filters.json";
    return filtersPath.string();
}

const FilterList& FilterManager::getFilters() const
{
    return m_filters;
}

FilterPtr FilterManager::getFilterByName(const std::string& name) const
{
    for (const auto& filter : m_filters)
    {
        if (filter && filter->name == name)
        {
            return filter;
        }
    }
    return nullptr;
}

} // namespace filters