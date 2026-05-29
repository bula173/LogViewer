#include "FieldTranslator.hpp"
#include "FieldConversionPluginRegistry.hpp"
#include "Logger.hpp"
#include "Config.hpp"

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

util::Result<void, error::Error> FieldTranslator::LoadFromFile(const std::string& filePath)
{
    using VoidResult = util::Result<void, error::Error>;
    util::Logger::Debug("FieldTranslator: loading dictionary from '{}'", filePath);
    try
    {
        std::ifstream file(filePath);
        if (!file.is_open())
        {
            util::Logger::Warn("FieldTranslator: could not open dictionary file '{}'", filePath);
            return VoidResult::Err(error::Error(error::ErrorCode::IOError,
                "Could not open dictionary file: " + filePath, /*showMsgBox=*/false));
        }

        json j;
        file >> j;

        if (!j.contains("translations") || !j["translations"].is_array())
        {
            util::Logger::Warn("FieldTranslator: invalid format in '{}' — missing 'translations' array",
                filePath);
            return VoidResult::Err(error::Error(error::ErrorCode::ParseError,
                "Invalid dictionary file format: missing 'translations' array in " + filePath,
                /*showMsgBox=*/false));
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

        // Rebuild the lowercase cache for fast lookups
        RebuildLowercaseCache();

        util::Logger::Debug("FieldTranslator: finished parsing '{}'", filePath);
        util::Logger::Info("FieldTranslator: loaded {} dictionary entry/entries from '{}'",
            m_dictionary.size(), filePath);
        return VoidResult::Ok({});
    }
    catch (const std::exception& ex)
    {
        util::Logger::Warn("FieldTranslator: exception loading dictionary from '{}': {}",
            filePath, ex.what());
        return VoidResult::Err(error::Error(error::ErrorCode::RuntimeError,
            std::string("Error loading dictionary from file '") + filePath + "': " + ex.what(),
            /*showMsgBox=*/false));
    }
}

std::string FieldTranslator::ToLower(std::string_view str) const
{
    std::string result(str);
    std::transform(result.begin(), result.end(), result.begin(),
        [](unsigned char c) { return std::tolower(c); });
    return result;
}

bool FieldTranslator::HasTranslation(std::string_view key) const
{
    std::string lowerKey = ToLower(key);
    return m_lowercaseKeyCache.find(lowerKey) != m_lowercaseKeyCache.end();
}

TranslationResult FieldTranslator::Translate(std::string_view key, std::string_view value) const
{
    TranslationResult result;
    result.convertedValue = std::string(value);
    result.wasConverted = false;

    // Case-insensitive key lookup using cache
    std::string lowerKey = ToLower(key);
    auto cacheIt = m_lowercaseKeyCache.find(lowerKey);
    if (cacheIt == m_lowercaseKeyCache.end())
    {
        util::Logger::Trace("FieldTranslator: no translation entry for key '{}'", key);
        return result;
    }

    util::Logger::Trace("FieldTranslator: translating key '{}' with conversionType '{}'",
        key, m_dictionary.at(cacheIt->second).conversionType);

    const auto& translation = m_dictionary.at(cacheIt->second);
    std::string converted = std::string(value);

    // Apply conversion using plugin system
    if (translation.conversionType != "tooltip_only")
    {
        const auto& registry = FieldConversionPluginRegistry::GetInstance();
        auto convResult = registry.ApplyConversion(translation.conversionType, value, translation.valueMap);

        if (convResult.isOk())
        {
            std::string pluginResult = convResult.unwrap();
            if (pluginResult != std::string(value))
            {
                converted = std::string(value) + " -> " + pluginResult;
                result.wasConverted = true;
            }
            else
            {
                result.wasConverted = false;
            }
        }
        else
        {
            // conversionType not found in registry — leave value unchanged
            util::Logger::Warn("FieldTranslator: conversion '{}' not available for key '{}': {}",
                translation.conversionType, key, convResult.error().what());
            result.wasConverted = false;
        }
    }
    else
    {
        // tooltip_only - no conversion, just provide tooltip
        result.wasConverted = false;
    }

    result.convertedValue = converted;

    // Generate tooltip if template is provided (regardless of conversion)
    if (!translation.tooltipTemplate.empty())
    {
        result.tooltip = FormatTooltip(translation.tooltipTemplate, value, converted);
    }

    return result;
}

void FieldTranslator::RebuildLowercaseCache() const
{
    m_lowercaseKeyCache.clear();
    for (const auto& [key, _] : m_dictionary)
    {
        m_lowercaseKeyCache[ToLower(key)] = key;
    }
}

std::string FieldTranslator::FormatTooltip(const std::string& templateStr,
    std::string_view original, std::string_view converted) const
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
    m_lowercaseKeyCache[ToLower(translation.key)] = translation.key;
}

void FieldTranslator::RemoveTranslation(const std::string& key)
{
    m_dictionary.erase(key);
    m_lowercaseKeyCache.erase(ToLower(key));
}

} // namespace config
