#pragma once

#include <nlohmann/json.hpp>
#include <map>
#include <string>
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
    bool HasTranslation(const std::string& key) const;

    /**
     * @brief Apply dictionary lookup/conversion to a field value
     * @param key The field key name
     * @param value The original value
     * @return TranslationResult with converted value and tooltip
     */
    TranslationResult Translate(const std::string& key, const std::string& value) const;
    
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
    std::string HexToAscii(const std::string& hexStr) const;
    std::string UnixToDate(const std::string& unixStr) const;
    std::string ApplyValueMap(const std::string& value, const std::map<std::string, std::string>& valueMap) const;
    std::string IsoLatin1ToUtf8(const std::string& text) const;
    std::string FormatTooltip(const std::string& templateStr, const std::string& original, const std::string& converted) const;
    std::string ToLower(const std::string& str) const;

    std::map<std::string, FieldDictionary> m_dictionary;
};

} // namespace config
