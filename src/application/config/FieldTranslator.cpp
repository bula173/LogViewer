#include "config/FieldTranslator.hpp"
#include "application/util/Logger.hpp"
#include "config/Config.hpp"

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

}

bool FieldTranslator::LoadFromFile(const std::string& filePath)
{
    try
    {
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            util::Logger::Error("Could not open dictionary file: {}", filePath);
            return false;
        }

        json j;
        file >> j;

        if (!j.contains("translations") || !j["translations"].is_array())
        {
            util::Logger::Error("Invalid dictionary file format: missing 'translations' array");
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
        util::Logger::Error("Error loading dictionary from file '{}': {}", filePath, ex.what());
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
    else if (translation.conversionType == "nid_lrbg")
    {
        converted = NidLrbg(value);
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

std::string FieldTranslator::NidLrbg(const std::string& text) const
{
    int nid = std::stoi(text);
    int nid_bg = (nid & 0x3fff);
    int nid_c = nid >> 14;

    return "nid_c=" + std::to_string(nid_c) + " nid_bg=" + std::to_string(nid_bg);
}

std::string FieldTranslator::IsoLatin1ToUtf8(const std::string& text) const
{
    int codepoint = std::stoi(text);
    
    // ISO Latin-1 codepoints are 0-255
    if (codepoint < 0 || codepoint > 255)
        return text; // Return original on invalid input
    
    // For UTF-8 encoding of codepoints 0-127, it's the same as ASCII/Latin-1
    if (codepoint <= 127)
        return std::string(1, static_cast<char>(codepoint));
    
    // For codepoints 128-255, UTF-8 uses 2 bytes
    // Format: 110xxxxx 10xxxxxx
    char utf8[3];
    utf8[0] = static_cast<char>(0xC0 | (codepoint >> 6));   // 110xxxxx
    utf8[1] = static_cast<char>(0x80 | (codepoint & 0x3F));  // 10xxxxxx
    utf8[2] = '\0';
    
    return std::string(utf8);
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
