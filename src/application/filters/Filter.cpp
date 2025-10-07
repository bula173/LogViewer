#include "Filter.hpp"
#include <spdlog/spdlog.h>

namespace filters
{

Filter::Filter(const std::string& name, const std::string& columnName,
    const std::string& pattern, bool caseSensitive, bool inverted,
    bool parameterFilter, const std::string& paramKey, int depth)
    : name(name)
    , columnName(columnName)
    , pattern(pattern)
    , isEnabled(true)
    , isInverted(inverted)
    , isCaseSensitive(caseSensitive)
    , isParameterFilter(parameterFilter)
    , parameterKey(paramKey)
    , parameterDepth(depth)
{
    compile();
}

bool Filter::matches(const std::string& value) const
{
    if (!regex)
        return false;

    bool matched = std::regex_search(value, *regex);
    return isInverted ? !matched : matched;
}

bool Filter::matchesParameter(const db::LogEvent& event) const
{
    if (!regex)
        return false;

    // For simple parameter search (depth = 0), just check direct fields
    if (parameterDepth == 0)
    {
        // Look for the parameter key in top-level fields
        std::string value = event.findByKey(parameterKey);
        if (!value.empty())
        {
            bool matched = std::regex_search(value, *regex);
            return isInverted ? !matched : matched;
        }
        return isInverted; // Not found, so match only if inverted
    }

    // For deeper searches, traverse through the event items
    auto items = event.getEventItems();
    return searchParameterRecursive(items, parameterKey, 0);
}

bool Filter::searchParameterRecursive(
    const std::vector<std::pair<std::string, std::string>>& items,
    const std::string& key, int currentDepth) const
{
    if (currentDepth > parameterDepth)
        return isInverted;

    for (const auto& item : items)
    {
        // Check if this item's key matches our target key
        if (item.first == key)
        {
            bool matched = std::regex_search(item.second, *regex);
            return isInverted ? !matched : matched;
        }

        // If this item might contain nested objects (JSON-like), parse and
        // search
        if (item.second.find('{') != std::string::npos &&
            item.second.find('}') != std::string::npos)
        {
            // Simple check for JSON-like content
            // In a real implementation, you'd use a proper JSON parser here
            std::vector<std::pair<std::string, std::string>> nestedItems;
            // Parse item.second into nestedItems (simplified for example)

            if (searchParameterRecursive(nestedItems, key, currentDepth + 1))
            {
                return true;
            }
        }
    }

    return isInverted; // Not found, so match only if inverted
}

nlohmann::json Filter::toJson() const
{
    nlohmann::json j;
    j["name"] = name;
    j["columnName"] = columnName;
    j["pattern"] = pattern;
    j["isEnabled"] = isEnabled;
    j["isInverted"] = isInverted;
    j["isCaseSensitive"] = isCaseSensitive;
    j["isParameterFilter"] = isParameterFilter;
    j["parameterKey"] = parameterKey;
    j["parameterDepth"] = parameterDepth;
    return j;
}

Filter Filter::fromJson(const nlohmann::json& j)
{
    Filter filter;
    filter.name = j["name"].get<std::string>();
    filter.columnName = j["columnName"].get<std::string>();
    filter.pattern = j["pattern"].get<std::string>();
    filter.isEnabled = j["isEnabled"].get<bool>();
    filter.isInverted = j["isInverted"].get<bool>();
    filter.isCaseSensitive = j["isCaseSensitive"].get<bool>();

    // Support legacy filters without parameter filtering
    if (j.contains("isParameterFilter"))
    {
        filter.isParameterFilter = j["isParameterFilter"].get<bool>();
        filter.parameterKey = j["parameterKey"].get<std::string>();
        filter.parameterDepth = j["parameterDepth"].get<int>();
    }
    else
    {
        filter.isParameterFilter = false;
        filter.parameterKey = "";
        filter.parameterDepth = 0;
    }

    filter.compile();
    return filter;
}

void Filter::compile()
{
    try
    {
        regex = std::make_shared<std::regex>(pattern,
            isCaseSensitive ? std::regex::ECMAScript
                            : std::regex::ECMAScript | std::regex::icase);
    }
    catch (const std::regex_error& e)
    {
        spdlog::error("Invalid regex pattern '{}': {}", pattern, e.what());
        regex = nullptr;
    }
}

} // namespace filters