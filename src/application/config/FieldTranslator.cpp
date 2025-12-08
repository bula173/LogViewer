#include "config/FieldTranslator.hpp"
#include "application/util/Logger.hpp"

#include <fstream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cctype>
#include <filesystem>
#include <vector>

namespace config
{

FieldTranslator::FieldTranslator()
{
    // Try to load default dictionary file
    std::vector<std::filesystem::path> searchPaths = {
        std::filesystem::current_path() / "etc" / "field_dictionary.json",
        std::filesystem::current_path() / "field_dictionary.json",
        std::filesystem::current_path() / "config" / "field_dictionary.json"
    };

    for (const auto& path : searchPaths)
    {
        if (std::filesystem::exists(path))
        {
            if (LoadFromFile(path.string()))
            {
                return;
            }
        }
    }

}

bool FieldTranslator::LoadFromFile(const std::string& filePath)
{
    try
    {
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            return false;
        }

        json j;
        file >> j;

        if (!j.contains("translations") || !j["translations"].is_array())
        {
            return false;
        }

        m_dictionary.clear();

        for (const auto& trans : j["translations"])
        {
            if (!trans.contains("key") || !trans["key"].is_string())
                continue;

            FieldDictionary ft;
            ft.key = trans["key"];
            ft.conversionType = trans.value("conversion", "none");
            ft.tooltipTemplate = trans.value("tooltip", "");

            // Load value map if present
            if (trans.contains("value_map") && trans["value_map"].is_object())
            {
                for (auto it = trans["value_map"].begin(); it != trans["value_map"].end(); ++it)
                {
                    ft.valueMap[it.key()] = it.value();
                }
            }

            m_dictionary[ft.key] = ft;
        }

        return true;
    }
    catch (const std::exception& ex)
    {
        return false;
    }
}

std::string FieldTranslator::ToLower(const std::string& str) const
{
    std::string result = str;
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return result;
}

bool FieldTranslator::HasTranslation(const std::string& key) const
{
    std::string lowerKey = ToLower(key);
    for (const auto& [dictKey, _] : m_dictionary)
    {
        if (ToLower(dictKey) == lowerKey)
            return true;
    }
    return false;
}

TranslationResult FieldTranslator::Translate(const std::string& key, const std::string& value) const
{
    TranslationResult result;
    result.convertedValue = value;
    result.wasConverted = false;

    // Case-insensitive key lookup
    std::string lowerKey = ToLower(key);
    auto it = m_dictionary.end();
    for (auto iter = m_dictionary.begin(); iter != m_dictionary.end(); ++iter)
    {
        if (ToLower(iter->first) == lowerKey)
        {
            it = iter;
            break;
        }
    }
    
    if (it == m_dictionary.end())
        return result;

    const auto& translation = it->second;
    std::string converted = value;

    // Apply conversion based on type
    if (translation.conversionType == "tooltip_only")
    {
        // No conversion, just provide tooltip
        result.wasConverted = false;
    }
    else if (translation.conversionType == "hex_to_ascii")
    {
        converted = HexToAscii(value);
        if (converted != value)
        {
            converted = value + " -> " + converted;
        }
        result.wasConverted = true;
    }
    else if (translation.conversionType == "unix_to_date")
    {
        converted = UnixToDate(value);
        if (converted != value)
        {
            converted = value + " -> " + converted;
        }
        result.wasConverted = true;
    }
    else if (translation.conversionType == "value_map")
    {
        converted = ApplyValueMap(value, translation.valueMap);
        if (converted != value)
        {
            converted = value + " -> " + converted;
            result.wasConverted = true;
        }
        else
        {
            result.wasConverted = false;
        }
    }
    else if (translation.conversionType == "iso_latin")
    {
        converted = IsoLatin1ToUtf8(value);
        if (converted != value)
        {
            converted = value + " -> " + converted;
            result.wasConverted = true;
        }
        else
        {
            result.wasConverted = false;
        }
    }

    result.convertedValue = converted;

    // Generate tooltip if template is provided (regardless of conversion)
    if (!translation.tooltipTemplate.empty())
    {
        result.tooltip = FormatTooltip(translation.tooltipTemplate, value, converted);
    }

    return result;
}

std::string FieldTranslator::HexToAscii(const std::string& hexStr) const
{
    if (hexStr.empty() || hexStr.length() % 2 != 0)
        return hexStr;

    // Check if all characters are hex digits
    bool isHex = std::all_of(hexStr.begin(), hexStr.end(),
        [](char c) { return std::isxdigit(static_cast<unsigned char>(c)); });

    if (!isHex)
        return hexStr;

    std::string ascii;
    ascii.reserve(hexStr.length() / 2);

    for (size_t i = 0; i < hexStr.length(); i += 2)
    {
        std::string byteStr = hexStr.substr(i, 2);
        try
        {
            char byte = static_cast<char>(std::stoi(byteStr, nullptr, 16));

            // Only include printable ASCII characters
            if (byte >= 32 && byte <= 126)
                ascii += byte;
            else
                ascii += '.';
        }
        catch (...)
        {
            return hexStr; // Return original on error
        }
    }

    return ascii;
}

std::string FieldTranslator::UnixToDate(const std::string& unixStr) const
{
    try
    {
        long long timestamp = std::stoll(unixStr);
        std::time_t time = static_cast<std::time_t>(timestamp);
        std::tm* tm = std::localtime(&time);

        if (tm)
        {
            std::ostringstream oss;
            oss << std::put_time(tm, "%Y-%m-%d %H:%M:%S");
            return oss.str();
        }
    }
    catch (...)
    {
        // Return original on error
    }

    return unixStr;
}

std::string FieldTranslator::IsoLatin1ToUtf8(const std::string& text) const
{
    // Convert numeric values or hex strings to ISO-8859-1 bytes, then to UTF-8
    // Supports formats like:
    // - "225" (decimal byte value)
    // - "E1" (hex byte value)
    // - "225 233 237" (space-separated decimal values)
    // - "E1 E9 ED" (space-separated hex values)

    std::vector<unsigned char> bytes;
    std::istringstream iss(text);
    std::string token;

    while (iss >> token)
    {
        try
        {
            unsigned long value;
            bool isHex = (token.length() > 0 && token[0] == '0' && token.length() > 1 && (token[1] == 'x' || token[1] == 'X')) ||
                        (token.length() > 1 && token.find_first_not_of("0123456789ABCDEFabcdef") == std::string::npos && token.length() <= 2);

            if (isHex && token.length() <= 2)
            {
                // Hex value like "E1" or "0xE1"
                value = std::stoul(token, nullptr, 16);
            }
            else
            {
                // Decimal value like "225"
                value = std::stoul(token, nullptr, 10);
            }

            if (value <= 0xFF)
            {
                bytes.push_back(static_cast<unsigned char>(value));
            }
        }
        catch (const std::exception&)
        {
            // If parsing fails, treat as direct ISO-8859-1 bytes
            for (char c : token)
            {
                bytes.push_back(static_cast<unsigned char>(c));
            }
        }
    }

    // If no numeric parsing succeeded, treat the entire string as ISO-8859-1 bytes
    if (bytes.empty())
    {
        for (char c : text)
        {
            bytes.push_back(static_cast<unsigned char>(c));
        }
    }

    // Convert bytes to UTF-8
    std::string result;
    result.reserve(bytes.size() * 2);

    for (unsigned char byte : bytes)
    {
        if (byte < 0x80)
        {
            // ASCII character (0x00-0x7F) - copy as-is
            result += static_cast<char>(byte);
        }
        else
        {
            // ISO-8859-1 character (0x80-0xFF) - convert to UTF-8
            char utf8[3];
            utf8[0] = static_cast<char>(0xC0 | (byte >> 6));   // First byte: 110xxxxx
            utf8[1] = static_cast<char>(0x80 | (byte & 0x3F));  // Second byte: 10xxxxxx
            utf8[2] = '\0';
            result += utf8[0];
            result += utf8[1];
        }
    }

    return result;
}

std::string FieldTranslator::ApplyValueMap(const std::string& value,
    const std::map<std::string, std::string>& valueMap) const
{
    auto it = valueMap.find(value);
    if (it != valueMap.end())
        return it->second;

    return value; // Return original if not found in map
}

std::string FieldTranslator::FormatTooltip(const std::string& templateStr,
    const std::string& original, const std::string& converted) const
{
    std::string result = templateStr;

    // Replace {original} placeholder
    size_t pos = 0;
    while ((pos = result.find("{original}", pos)) != std::string::npos)
    {
        result.replace(pos, 10, original);
        pos += original.length();
    }

    // Replace {converted} placeholder
    pos = 0;
    while ((pos = result.find("{converted}", pos)) != std::string::npos)
    {
        result.replace(pos, 11, converted);
        pos += converted.length();
    }

    return result;
}

const std::map<std::string, FieldDictionary>& FieldTranslator::GetAllTranslations() const
{
    return m_dictionary;
}

bool FieldTranslator::SaveToFile(const std::string& filePath) const
{
    try
    {
        json j;
        j["translations"] = json::array();

        for (const auto& [key, trans] : m_dictionary)
        {
            json entry;
            entry["key"] = trans.key;
            entry["conversion"] = trans.conversionType;
            if (!trans.tooltipTemplate.empty())
            {
                entry["tooltip"] = trans.tooltipTemplate;
            }
            if (!trans.valueMap.empty())
            {
                entry["value_map"] = trans.valueMap;
            }
            j["translations"].push_back(entry);
        }

        std::ofstream file(filePath);
        if (!file.is_open())
        {
            util::Logger::Error("Failed to open dictionary file for writing: {}", filePath);
            return false;
        }

        file << j.dump(2);
        file.close();

        util::Logger::Info("Saved {} dictionary entrie(s) to {}", m_dictionary.size(), filePath);
        return true;
    }
    catch (const std::exception& ex)
    {
        util::Logger::Error("Failed to save dictionary: {}", ex.what());
        return false;
    }
}

void FieldTranslator::SetTranslation(const FieldDictionary& translation)
{
    m_dictionary[translation.key] = translation;
}

void FieldTranslator::RemoveTranslation(const std::string& key)
{
    m_dictionary.erase(key);
}

} // namespace config
