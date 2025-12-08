#include "config/FieldTranslator.hpp"
#include "application/util/Logger.hpp"

#include <fstream>
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cctype>
#include <filesystem>

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
        converted = ToIsoLatin(value);
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

std::string FieldTranslator::ToIsoLatin(const std::string& text) const
{
    // Map of common UTF-8 encoded characters to ASCII equivalents
    static const std::map<std::string, std::string> latinToAscii = {
        // Polish characters
        {"ą", "a"}, {"Ą", "A"},
        {"ć", "c"}, {"Ć", "C"},
        {"ę", "e"}, {"Ę", "E"},
        {"ł", "l"}, {"Ł", "L"},
        {"ń", "n"}, {"Ń", "N"},
        {"ó", "o"}, {"Ó", "O"},
        {"ś", "s"}, {"Ś", "S"},
        {"ź", "z"}, {"Ź", "Z"},
        {"ż", "z"}, {"Ż", "Z"},
        
        // German characters
        {"ä", "ae"}, {"Ä", "Ae"},
        {"ö", "oe"}, {"Ö", "Oe"},
        {"ü", "ue"}, {"Ü", "Ue"},
        {"ß", "ss"},
        
        // French characters
        {"à", "a"}, {"À", "A"},
        {"â", "a"}, {"Â", "A"},
        {"æ", "ae"}, {"Æ", "Ae"},
        {"ç", "c"}, {"Ç", "C"},
        {"é", "e"}, {"É", "E"},
        {"è", "e"}, {"È", "E"},
        {"ê", "e"}, {"Ê", "E"},
        {"ë", "e"}, {"Ë", "E"},
        {"î", "i"}, {"Î", "I"},
        {"ï", "i"}, {"Ï", "I"},
        {"ô", "o"}, {"Ô", "O"},
        {"œ", "oe"}, {"Œ", "Oe"},
        {"ù", "u"}, {"Ù", "U"},
        {"û", "u"}, {"Û", "U"},
        {"ü", "u"}, {"Ü", "U"},
        {"ÿ", "y"}, {"Ÿ", "Y"},
        
        // Spanish characters
        {"á", "a"}, {"Á", "A"},
        {"í", "i"}, {"Í", "I"},
        {"ñ", "n"}, {"Ñ", "N"},
        {"ú", "u"}, {"Ú", "U"},
        {"¿", "?"}, {"¡", "!"},
        
        // Italian characters
        {"ì", "i"}, {"Ì", "I"},
        {"ò", "o"}, {"Ò", "O"},
        
        // Scandinavian characters
        {"å", "a"}, {"Å", "A"},
        {"ø", "o"}, {"Ø", "O"},
        
        // Czech characters
        {"č", "c"}, {"Č", "C"},
        {"ď", "d"}, {"Ď", "D"},
        {"ě", "e"}, {"Ě", "E"},
        {"ň", "n"}, {"Ň", "N"},
        {"ř", "r"}, {"Ř", "R"},
        {"š", "s"}, {"Š", "S"},
        {"ť", "t"}, {"Ť", "T"},
        {"ů", "u"}, {"Ů", "U"},
        {"ý", "y"}, {"Ý", "Y"},
        {"ž", "z"}, {"Ž", "Z"},
        
        // Other common characters
        {"–", "-"}, {"—", "-"},  // dashes
        {"\xE2\x80\x98", "'"}, {"\xE2\x80\x99", "'"}, // single quotes
        {"\xE2\x80\x9C", "\""}, {"\xE2\x80\x9D", "\""}, // double quotes
        {"…", "..."}, // ellipsis
    };
    
    std::string result;
    result.reserve(text.length());
    
    size_t i = 0;
    while (i < text.length())
    {
        // Try to match multi-byte UTF-8 sequences
        bool matched = false;
        
        // Try matching 2, 3, or 4 byte sequences
        for (size_t len = 4; len >= 2 && i + len <= text.length(); --len)
        {
            std::string substring = text.substr(i, len);
            auto it = latinToAscii.find(substring);
            if (it != latinToAscii.end())
            {
                result += it->second;
                i += len;
                matched = true;
                break;
            }
        }
        
        if (!matched)
        {
            // No match found, copy the byte as-is
            result += text[i];
            ++i;
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
