#pragma once

#include "EvlogTemplate.hpp"

#include <filesystem>
#include <map>
#include <utility>

namespace parser {

/// Loads and indexes evlog template files keyed by (facility, event_type).
///
/// Template files use the evlog native format:
///
///   facility    LOG_USER          (or numeric, e.g. 8)
///   event_type  0x00000001        (or decimal)
///   description "Login event"
///   attributes {
///       int   uid;
///       char  hostname[64];
///   }
///   format "uid=%uid% host=%hostname%"
///
/// Multiple templates may appear in one file, separated by "---".
/// Keywords may also appear with percent-sign delimiters (%facility% etc.).
/// Lines starting with '#' or '//' are treated as comments.
class EvlogTemplateRegistry {
public:
    /// Load every *.t file found directly inside @p dir (non-recursive).
    void LoadFromDirectory(const std::filesystem::path& dir);

    /// Load a single template file (may contain multiple templates).
    void LoadFromFile(const std::filesystem::path& path);

    /// Look up template by (facility, event_type); nullptr if not found.
    const EvlogTemplate* Find(int32_t facility, uint32_t eventType) const;

    size_t Count() const { return m_templates.size(); }

private:
    std::map<std::pair<int32_t, uint32_t>, EvlogTemplate> m_templates;
};

} // namespace parser
