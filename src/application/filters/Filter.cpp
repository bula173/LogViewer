#include "Filter.hpp"
#include "Logger.hpp"
#include "Error.hpp"

namespace filters
{

// FilterCondition implementation
FilterCondition::FilterCondition(const FilterCondition& other)
    : columnName(other.columnName)
    , pattern(other.pattern)
    , isParameterFilter(other.isParameterFilter)
    , parameterKey(other.parameterKey)
    , parameterDepth(other.parameterDepth)
    , isCaseSensitive(other.isCaseSensitive)
    , strategy(other.strategy ? other.strategy->clone() : nullptr)
{
}

FilterCondition& FilterCondition::operator=(const FilterCondition& other)
{
    if (this != &other) {
        columnName = other.columnName;
        pattern = other.pattern;
        isParameterFilter = other.isParameterFilter;
        parameterKey = other.parameterKey;
        parameterDepth = other.parameterDepth;
        isCaseSensitive = other.isCaseSensitive;
        strategy = other.strategy ? other.strategy->clone() : nullptr;
    }
    return *this;
}

bool FilterCondition::matches(const db::LogEvent& event) const
{
    if (isParameterFilter) {
        // Parameter matching logic
        if (parameterDepth == 0) {
            // For depth 0, use findByKey like the legacy code
            std::string value = event.findByKey(parameterKey);
            if (!value.empty()) {
                return strategy && strategy->matches(value, pattern, isCaseSensitive);
            }
            return false; // Key not found
        } else {
            // For deeper searches, traverse through the event items
            const auto& items = event.getEventItems();
            return searchParameterRecursive(items, parameterKey, 0);
        }
    } else {
        // Regular column matching
        if (columnName == "*") {
            // Check all fields
            for (const auto& field : event.getEventItems()) {
                const std::string& value = event.findByKey(field.first);
                if (strategy && strategy->matches(value, pattern, isCaseSensitive)) {
                    return true;
                }
            }
            return false;
        } else {
            // Check specific field
            std::string valueToCheck = event.findByKey(columnName);
            return strategy && strategy->matches(valueToCheck, pattern, isCaseSensitive);
        }
    }
}

bool FilterCondition::searchParameterRecursive(
    const std::vector<std::pair<std::string, std::string>>& items,
    const std::string& key, int currentDepth) const
{
    if (currentDepth > parameterDepth && parameterDepth != 0)
        return false;

    for (const auto& item : items) {
        if (item.first == key) {
            return strategy && strategy->matches(item.second, pattern, isCaseSensitive);
        }
    }
    return false;
}

nlohmann::json FilterCondition::toJson() const
{
    nlohmann::json j;
    j["columnName"] = columnName;
    j["pattern"] = pattern;
    j["isParameterFilter"] = isParameterFilter;
    j["parameterKey"] = parameterKey;
    j["parameterDepth"] = parameterDepth;
    j["isCaseSensitive"] = isCaseSensitive;
    j["strategy"] = strategy ? strategy->getName() : "regex";
    return j;
}

FilterCondition FilterCondition::fromJson(const nlohmann::json& j)
{
    FilterCondition condition;
    condition.columnName = j.value("columnName", "");
    condition.pattern = j.value("pattern", "");
    condition.isParameterFilter = j.value("isParameterFilter", false);
    condition.parameterKey = j.value("parameterKey", "");
    condition.parameterDepth = j.value("parameterDepth", 0);
    condition.isCaseSensitive = j.value("isCaseSensitive", false);
    
    std::string strategyName = j.value("strategy", "regex");
    condition.strategy = createStrategy(strategyName);
    
    return condition;
}

// Filter implementation
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
    // For backward compatibility, create a single condition from the legacy fields
    if (!columnName.empty() || !pattern.empty()) {
        FilterCondition condition(columnName, pattern, parameterFilter, paramKey, depth, isCaseSensitive);
        condition.strategy = std::make_unique<RegexFilterStrategy>();
        conditions.push_back(std::move(condition));
    }
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
    , conditions(other.conditions) // Copy conditions
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
        conditions = other.conditions; // Copy conditions
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

bool Filter::matches(const db::LogEvent& event) const
{
    // If we have multiple conditions, ALL must match (AND logic)
    if (!conditions.empty()) {
        for (const auto& condition : conditions) {
            if (!condition.matches(event)) {
                return isInverted; // If any condition fails, filter fails (unless inverted)
            }
        }
        return !isInverted; // All conditions passed
    }
    
    // Fallback to legacy single-condition logic for backward compatibility
    if (isParameterFilter) {
        return matchesParameter(event);
    } else {
        if (columnName == "*") {
            // Check all fields
            for (const auto& field : event.getEventItems()) {
                const std::string& value = event.findByKey(field.first);
                if (matches(value)) {
                    return true;
                }
            }
            return false;
        } else {
            // Check specific field
            std::string valueToCheck = event.findByKey(columnName);
            return matches(valueToCheck);
        }
    }
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

std::vector<unsigned long> Filter::applyToIndices(const std::vector<unsigned long>& inputIndices, const mvc::IModel& model) const
{
    if (conditions.empty()) {
        // Fallback to legacy logic
        std::vector<unsigned long> result;
        for (unsigned long index : inputIndices) {
            const auto& event = model.GetItem(static_cast<int>(index));
            if (matches(event)) {
                result.push_back(index);
            }
        }
        return result;
    }

    // Sequential filtering for multiple conditions (AND logic)
    auto temp = inputIndices;
    for (const auto& condition : conditions) {
        std::vector<unsigned long> new_temp;
        for (unsigned long index : temp) {
            const auto& event = model.GetItem(static_cast<int>(index));
            if (condition.matches(event)) {
                new_temp.push_back(index);
            }
        }
        temp = std::move(new_temp);
    }
    return temp;
}

void Filter::addCondition(const FilterCondition& condition)
{
    conditions.push_back(condition);
}

void Filter::removeCondition(size_t index)
{
    if (index < conditions.size()) {
        conditions.erase(std::next(conditions.begin(), static_cast<std::ptrdiff_t>(index)));
    }
}

void Filter::clearConditions()
{
    conditions.clear();
}

const std::vector<FilterCondition>& Filter::getConditions() const
{
    return conditions;
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
    
    // Persist conditions
    if (!conditions.empty()) {
        nlohmann::json conditionsJson = nlohmann::json::array();
        for (const auto& condition : conditions) {
            conditionsJson.push_back(condition.toJson());
        }
        j["conditions"] = conditionsJson;
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

    // Load conditions if present (new format)
    if (j.contains("conditions") && j["conditions"].is_array())
    {
        for (const auto& conditionJson : j["conditions"])
        {
            filter.conditions.push_back(FilterCondition::fromJson(conditionJson));
        }
    }
    else
    {
        // Backward compatibility: create single condition from legacy fields
        if (!filter.columnName.empty() || !filter.pattern.empty())
        {
            FilterCondition condition(filter.columnName, filter.pattern, 
                                    filter.isParameterFilter, filter.parameterKey, filter.parameterDepth,
                                    filter.isCaseSensitive);
            condition.strategy = filter.m_strategy ? filter.m_strategy->clone() : std::make_unique<RegexFilterStrategy>();
            filter.conditions.push_back(std::move(condition));
        }
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