#pragma once

#include <nlohmann/json.hpp>
#include <map>
#include <string>
#include <string_view>
#include <optional>
#include <functional>

namespace config
{

using json = nlohmann::json;

/**
 * @brief Configuration for a single field dictionary entry/conversion
 */
struct FieldDictionary
{
    std::string key;                                    // Field key to match
    std::string conversionType;                         // Type of conversion (hex_to_ascii, value_map, etc.)
    std::string tooltipTemplate;                        // Tooltip format string with {original} and {converted}
    std::map<std::string, std::string> valueMap;       // For value_map conversions
};

/**
 * @brief Result of a field dictionary lookup/conversion
 */
struct TranslationResult
{
    std::string convertedValue;
    std::string tooltip;
    bool wasConverted {false};
};

/**
 * @brief Manages field dictionary and conversions based on JSON configuration
 */
class FieldTranslator
{
public:
    FieldTranslator();
    ~FieldTranslator() = default;

    /**
     * @brief Load dictionary from JSON file
     * @param filePath Path to the dictionary JSON file
     * @return true if loaded successfully
     */
    bool LoadFromFile(const std::string& filePath);

    /**
     * @brief Check if a field has a dictionary entry configured
     * @param key The field key name
     * @return true if dictionary entry exists for this key
     */
    bool HasTranslation(std::string_view key) const;

    /**
     * @brief Apply dictionary lookup/conversion to a field value
     * @param key The field key name
     * @param value The original value
     * @return TranslationResult with converted value and tooltip
     */
    TranslationResult Translate(std::string_view key, std::string_view value) const;
    
    /**
     * @brief Get all dictionary entries
     * @return Map of all field dictionary entries
     */
    const std::map<std::string, FieldDictionary>& GetAllTranslations() const;
    
    /**
     * @brief Save dictionary to JSON file
     * @param filePath Path to save the dictionary JSON file
     * @return true if saved successfully
     */
    bool SaveToFile(const std::string& filePath) const;
    
    /**
     * @brief Add or update a dictionary entry
     * @param translation The dictionary entry to add/update
     */
    void SetTranslation(const FieldDictionary& translation);
    
    /**
     * @brief Remove a dictionary entry
     * @param key The field key to remove
     */
    void RemoveTranslation(const std::string& key);

private:

    std::string HexToAscii(std::string_view hexStr) const;
    std::string UnixToDate(std::string_view unixStr) const;
    std::string ApplyValueMap(std::string_view value, const std::map<std::string, std::string>& valueMap) const;
    std::string IsoLatin1ToUtf8(std::string_view text) const;
    std::string FormatTooltip(const std::string& templateStr, std::string_view original, std::string_view converted) const;
    std::string ToLower(std::string_view str) const;
    std::string NidLrbg(std::string_view text) const;
    
    // Rebuild the lowercase key cache for fast case-insensitive lookups
    void RebuildLowercaseCache() const;
    
    // Cache for case-insensitive lookups (maps lowercase key to original key)
    mutable std::map<std::string, std::string> m_lowercaseKeyCache;
    std::map<std::string, FieldDictionary> m_dictionary;
};

} // namespace config
