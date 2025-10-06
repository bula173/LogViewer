#include "Filter.hpp"
#include <spdlog/spdlog.h>

namespace filters
{

Filter::Filter(const std::string& name, const std::string& columnName,
    const std::string& pattern, bool caseSensitive, bool inverted)
    : name(name)
    , columnName(columnName)
    , pattern(pattern)
    , isEnabled(true)
    , isInverted(inverted)
    , isCaseSensitive(caseSensitive)
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

void Filter::compile()
{
    try
    {
        std::regex_constants::syntax_option_type flags =
            std::regex_constants::ECMAScript;

        if (!isCaseSensitive)
            flags |= std::regex_constants::icase;

        regex = std::make_shared<std::regex>(pattern, flags);
    }
    catch (const std::regex_error& e)
    {
        spdlog::error(
            "Invalid regex pattern in filter '{}': {}", name, e.what());
        regex.reset();
    }
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
    filter.compile();
    return filter;
}

} // namespace filters