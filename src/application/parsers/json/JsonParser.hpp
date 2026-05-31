#pragma once

#include "IDataParser.hpp"

#include <cstdint>
#include <filesystem>
#include <istream>
#include <string>
#include <string_view>

#include <nlohmann/json.hpp>

namespace parser
{

/// Parses JSON log files in two formats:
///
///   Array      — a single JSON array of objects at the top level.
///                [ {"ts": "...", "level": "INFO", "msg": "..."}, ... ]
///
///   NDJSON     — one JSON object per line (newline-delimited JSON / JSONL).
///                {"ts": "...", "level": "INFO", "msg": "..."}
///                {"ts": "...", "level": "WARN", "msg": "..."}
///
/// The format is detected automatically: the parser reads the first
/// non-whitespace character; '[' means array mode, '{' means NDJSON mode.
///
/// Nested objects and arrays are flattened using dot-notation keys:
///   { "ctx": { "user": "alice" } }  →  key "ctx.user" = "alice"
///   { "tags": ["a","b"] }           →  key "tags.0" = "a", "tags.1" = "b"
class JsonParser : public IDataParser
{
public:
    JsonParser() = default;
    ~JsonParser() override = default;

    void ParseData(const std::filesystem::path& filepath) override;
    void ParseData(std::istream& input) override;

    uint32_t GetCurrentProgress() const override { return m_currentProgress; }
    uint32_t GetTotalProgress()   const override { return m_totalProgress; }

private:
    void EmitObject(const nlohmann::json& obj, int id);
    void Flatten(const nlohmann::json& node,
                 const std::string& prefix,
                 db::LogEvent::EventItems& out) const;

    uint32_t m_currentProgress {0};
    uint32_t m_totalProgress   {0};
};

} // namespace parser
