#pragma once

#include <nlohmann/json.hpp>
#include <string>
#include <vector>

namespace ui::qt {

/**
 * @brief Describes a named actor matched by a regular expression.
 *
 * The regexp is tested against the value of @p field in each log event.
 * Leave @p field empty to match against the full serialised event (all
 * field values concatenated with a space).
 */
struct ActorDefinition
{
    std::string name;     ///< Display name shown in the Actors panel
    std::string pattern;  ///< ECMAScript/QRegularExpression pattern
    std::string field;    ///< Log field to match against (empty = any field)
    bool        enabled {true};

    // ── JSON round-trip ──────────────────────────────────────────────────
    [[nodiscard]] nlohmann::json ToJson() const
    {
        return {{"name", name}, {"pattern", pattern},
                {"field", field}, {"enabled", enabled}};
    }

    static ActorDefinition FromJson(const nlohmann::json& j)
    {
        ActorDefinition d;
        d.name    = j.value("name",    "");
        d.pattern = j.value("pattern", "");
        d.field   = j.value("field",   "");
        d.enabled = j.value("enabled", true);
        return d;
    }

    // ── Serialise a full list ────────────────────────────────────────────
    static nlohmann::json ListToJson(const std::vector<ActorDefinition>& defs)
    {
        nlohmann::json arr = nlohmann::json::array();
        for (const auto& d : defs) arr.push_back(d.ToJson());
        return arr;
    }

    static std::vector<ActorDefinition> ListFromJson(const nlohmann::json& j)
    {
        std::vector<ActorDefinition> defs;
        if (!j.is_array()) return defs;
        for (const auto& item : j)
        {
            try { defs.push_back(FromJson(item)); } catch (...) {}
        }
        return defs;
    }
};

} // namespace ui::qt
