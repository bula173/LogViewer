#include "Filter.hpp"
#include "util/Logger.hpp"
#include "error/Error.hpp"

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
    , m_strategy(std::make_unique<RegexFilterStrategy>()) // Default strategy
{
    compile();
}

// Copy constructor - clone the strategy
Filter::Filter(const Filter& other)
    : name(other.name)
    , columnName(other.columnName)
    , pattern(other.pattern)
    , isEnabled(other.isEnabled)
    , isInverted(other.isInverted)
    , isCaseSensitive(other.isCaseSensitive)
    , isParameterFilter(other.isParameterFilter)
    , parameterKey(other.parameterKey)
    , parameterDepth(other.parameterDepth)
    , regex(other.regex) // shared_ptr is safe to copy
    , m_strategy(other.m_strategy ? other.m_strategy->clone() : nullptr)
{
}

// Copy assignment
Filter& Filter::operator=(const Filter& other)
{
    if (this != &other) {
        name = other.name;
        columnName = other.columnName;
        pattern = other.pattern;
        isEnabled = other.isEnabled;
        isInverted = other.isInverted;
        isCaseSensitive = other.isCaseSensitive;
        isParameterFilter = other.isParameterFilter;
        parameterKey = other.parameterKey;
        parameterDepth = other.parameterDepth;
        regex = other.regex;
        m_strategy = other.m_strategy ? other.m_strategy->clone() : nullptr;
    }
    return *this;
}

bool Filter::matches(const std::string& value) const
{
    // Use strategy if available, fallback to legacy regex
    if (m_strategy) {
        bool matched = m_strategy->matches(value, pattern, isCaseSensitive);
        return isInverted ? !matched : matched;
    }

    // Legacy path: use compiled regex
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
    
    // Persist strategy type
    if (m_strategy) {
        j["strategy"] = m_strategy->getName();
    }
    
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

    // Restore strategy if persisted
    if (j.contains("strategy"))
    {
        std::string strategyName = j["strategy"].get<std::string>();
        filter.m_strategy = createStrategy(strategyName);
    }
    else
    {
        filter.m_strategy = std::make_unique<RegexFilterStrategy>();
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
        util::Logger::Error("Invalid regex pattern '{}': {}", pattern, e.what());
        regex = nullptr;
    }
}

void Filter::setStrategy(std::unique_ptr<IFilterStrategy> strategy)
{
    if (!strategy) {
        util::Logger::Warn("Filter::setStrategy - Null strategy provided, using default regex strategy");
        m_strategy = std::make_unique<RegexFilterStrategy>();
        return;
    }

    // Validate pattern with new strategy
    if (!strategy->isValidPattern(pattern)) {
        util::Logger::Error("Filter::setStrategy - Pattern '{}' is invalid for strategy '{}'",
            pattern, strategy->getName());
        throw error::Error(error::ErrorCode::InvalidArgument, "Pattern incompatible with strategy");
    }

    util::Logger::Debug("Filter::setStrategy - Switching to strategy '{}'", strategy->getName());
    m_strategy = std::move(strategy);
}

std::string Filter::getStrategyName() const
{
    return m_strategy ? m_strategy->getName() : "none";
}

} // namespace filters