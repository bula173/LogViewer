#include "FilterManager.hpp"
#include <filesystem>
#include <fstream>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>

#include "config/Config.hpp"

namespace filters
{

FilterManager& FilterManager::getInstance()
{
    static FilterManager instance;
    return instance;
}

FilterManager::FilterManager()
{
    loadFilters();
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
            spdlog::error("Filter with name '{}' already exists", filter->name);
            return;
        }
    }

    m_filters.push_back(filter);
    spdlog::debug("Added filter: {}", filter->name);
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
            spdlog::debug("Updated filter: {}", filter->name);
            return;
        }
    }

    // If not found, add as new
    m_filters.push_back(filter);
    spdlog::debug("Added filter (during update): {}", filter->name);
}

void FilterManager::removeFilter(const std::string& filterName)
{
    auto it = std::find_if(m_filters.begin(), m_filters.end(),
        [&](const FilterPtr& f) { return f->name == filterName; });

    if (it != m_filters.end())
    {
        m_filters.erase(it);
        spdlog::debug("Removed filter: {}", filterName);
    }
}

void FilterManager::enableFilter(const std::string& filterName, bool enable)
{
    for (auto& filter : m_filters)
    {
        if (filter->name == filterName)
        {
            filter->isEnabled = enable;
            spdlog::debug(
                "{} filter: {}", enable ? "Enabled" : "Disabled", filterName);
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
    spdlog::debug("{} all filters", enable ? "Enabled" : "Disabled");
}

std::vector<size_t> FilterManager::applyFilters(
    db::EventsContainer& container) const
{
    std::vector<size_t> result;
    result.reserve(container.Size());

    // Count enabled filters
    size_t enabledFilters = 0;
    for (const auto& filter : m_filters)
    {
        if (filter && filter->isEnabled)
            enabledFilters++;
    }

    // If no filters enabled, include all events
    if (enabledFilters == 0)
    {
        for (size_t i = 0; i < container.Size(); ++i)
        {
            result.push_back(i);
        }
        return result;
    }

    // Apply filters to each event using OR logic
    for (size_t i = 0; i < container.Size(); ++i)
    {
        const auto& event = container.GetEvent(i);
        bool passesAnyFilter = false;

        for (const auto& filter : m_filters)
        {
            if (!filter || !filter->isEnabled)
                continue;

            // Special case for wildcard column - check all fields
            if (filter->columnName == "*")
            {
                for (const auto& field : event.getEventItems())
                {
                    const std::string& value = event.findByKey(field.first);
                    if (filter->matches(value))
                    {
                        passesAnyFilter = true;
                        break;
                    }
                }
            }
            else
            {
                // Check specific field
                std::string valueToCheck = event.findByKey(filter->columnName);
                if (filter->matches(valueToCheck))
                {
                    passesAnyFilter = true;
                    break; // Found one match, no need to check other filters
                }
            }

            // If any filter passes, we can stop checking
            if (passesAnyFilter)
                break;
        }

        if (passesAnyFilter)
        {
            result.push_back(i);
        }
    }

    return result;
}

void FilterManager::loadFilters()
{
    std::string filePath = getFiltersFilePath();

    if (!std::filesystem::exists(filePath))
    {
        spdlog::info("Filters file does not exist at: {}", filePath);
        return;
    }

    try
    {
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            spdlog::error("Failed to open filters file: {}", filePath);
            return;
        }

        nlohmann::json j;
        file >> j;

        m_filters.clear();

        if (j.is_array())
        {
            for (const auto& filterJson : j)
            {
                try
                {
                    Filter filter = Filter::fromJson(filterJson);
                    m_filters.push_back(std::make_shared<Filter>(filter));
                }
                catch (const std::exception& e)
                {
                    spdlog::error("Error parsing filter: {}", e.what());
                }
            }
        }

        spdlog::info("Loaded {} filters from {}", m_filters.size(), filePath);
    }
    catch (const std::exception& e)
    {
        spdlog::error("Error loading filters: {}", e.what());
    }
}

void FilterManager::saveFilters()
{
    std::string filePath = getFiltersFilePath();

    try
    {
        // Create directory if it doesn't exist
        std::filesystem::path path(filePath);
        std::filesystem::create_directories(path.parent_path());

        nlohmann::json j = nlohmann::json::array();
        for (const auto& filter : m_filters)
        {
            if (filter)
            {
                j.push_back(filter->toJson());
            }
        }

        std::ofstream file(filePath);
        if (!file.is_open())
        {
            spdlog::error(
                "Failed to open filters file for writing: {}", filePath);
            return;
        }

        file << j.dump(2); // Pretty print with 2-space indent
        spdlog::info("Saved {} filters to {}", m_filters.size(), filePath);
    }
    catch (const std::exception& e)
    {
        spdlog::error("Error saving filters: {}", e.what());
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