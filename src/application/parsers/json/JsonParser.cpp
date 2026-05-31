#include "JsonParser.hpp"

#include "Error.hpp"
#include "LogEvent.hpp"
#include "Logger.hpp"

#include <fstream>
#include <sstream>
#include <string>

namespace parser
{

// ---------------------------------------------------------------------------
// ParseData — file
// ---------------------------------------------------------------------------

void JsonParser::ParseData(const std::filesystem::path& filepath)
{
    util::Logger::Debug("JsonParser::ParseData opening '{}'", filepath.string());

    std::ifstream file(filepath, std::ios::binary);
    if (!file.is_open())
    {
        throw error::Error(error::ErrorCode::FileNotFound,
            "JsonParser: cannot open file: " + filepath.string());
    }

    file.seekg(0, std::ios::end);
    m_totalProgress = static_cast<uint32_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    m_currentProgress = 0;

    ParseData(file);
}

// ---------------------------------------------------------------------------
// ParseData — stream
// ---------------------------------------------------------------------------

void JsonParser::ParseData(std::istream& input)
{
    // Peek at the first non-whitespace character to decide the format.
    char first = '\0';
    while (input.get(first) && (first == ' ' || first == '\t' ||
                                 first == '\r' || first == '\n')) {}

    if (!input.good())
    {
        util::Logger::Warn("JsonParser: empty or unreadable stream");
        return;
    }

    input.putback(first);

    if (first == '[')
    {
        // ── Array format ────────────────────────────────────────────────────
        util::Logger::Debug("JsonParser: detected array format");
        nlohmann::json doc;
        try {
            input >> doc;
        } catch (const nlohmann::json::exception& ex) {
            throw error::Error(error::ErrorCode::ParseError,
                std::string("JsonParser: JSON parse error: ") + ex.what());
        }

        if (!doc.is_array())
        {
            throw error::Error(error::ErrorCode::ParseError,
                "JsonParser: expected a JSON array at top level");
        }

        int id = 0;
        for (const auto& element : doc)
        {
            if (element.is_object())
                EmitObject(element, ++id);
        }
        m_currentProgress = m_totalProgress;
        util::Logger::Debug("JsonParser: parsed {} events (array format)", id);
    }
    else if (first == '{')
    {
        // ── NDJSON format ────────────────────────────────────────────────────
        util::Logger::Debug("JsonParser: detected NDJSON format");
        std::string line;
        int id = 0;
        while (std::getline(input, line))
        {
            // Strip \r in case of CRLF files.
            if (!line.empty() && line.back() == '\r')
                line.pop_back();
            if (line.empty())
                continue;

            try {
                auto obj = nlohmann::json::parse(line);
                if (obj.is_object())
                    EmitObject(obj, ++id);
            } catch (const nlohmann::json::exception& ex) {
                util::Logger::Warn("JsonParser: skipping malformed line {}: {}",
                    id + 1, ex.what());
            }

            m_currentProgress += static_cast<uint32_t>(line.size() + 1);
        }
        util::Logger::Debug("JsonParser: parsed {} events (NDJSON format)", id);
    }
    else
    {
        throw error::Error(error::ErrorCode::ParseError,
            std::string("JsonParser: unexpected first character '") + first +
            "'; expected '[' (array) or '{' (NDJSON)");
    }
}

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

void JsonParser::EmitObject(const nlohmann::json& obj, int id)
{
    db::LogEvent::EventItems items;
    Flatten(obj, "", items);
    NotifyNewEvent(db::LogEvent(id, std::move(items)));
}

void JsonParser::Flatten(const nlohmann::json& node,
                         const std::string& prefix,
                         db::LogEvent::EventItems& out) const
{
    if (node.is_object())
    {
        for (auto it = node.begin(); it != node.end(); ++it)
        {
            const std::string key = prefix.empty() ? it.key()
                                                   : prefix + "." + it.key();
            Flatten(it.value(), key, out);
        }
    }
    else if (node.is_array())
    {
        for (size_t i = 0; i < node.size(); ++i)
        {
            const std::string key = prefix + "." + std::to_string(i);
            Flatten(node[i], key, out);
        }
    }
    else if (node.is_string())
    {
        out.emplace_back(prefix, node.get<std::string>());
    }
    else if (node.is_boolean())
    {
        out.emplace_back(prefix, node.get<bool>() ? "true" : "false");
    }
    else if (node.is_null())
    {
        out.emplace_back(prefix, "");
    }
    else
    {
        // number (int or float)
        out.emplace_back(prefix, node.dump());
    }
}

} // namespace parser
