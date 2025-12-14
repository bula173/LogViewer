#include "plugins/IFieldConversionPlugin.hpp"
#include "config/FieldConversionPluginRegistry.hpp"
#include <algorithm>
#include <sstream>
#include <iomanip>
#include <ctime>
#include <cctype>

namespace config
{

/**
 * @brief Register all built-in field conversion plugins
 */
void RegisterBuiltinConversionPlugins();

/**
 * @brief Built-in hex to ASCII conversion plugin
 */
class HexToAsciiPlugin : public plugin::IFieldConversionPlugin
{
public:
    std::string GetConversionType() const override { return "hex_to_ascii"; }
    std::string GetDescription() const override { return "Convert hexadecimal string to ASCII text"; }
    std::string GetConversionName() const override { return "Hex to ASCII"; }

    std::string Convert(std::string_view value, const std::map<std::string, std::string>& /*parameters*/) const override
    {
        if (value.empty() || value.length() % 2 != 0)
            return std::string(value);

        // Check if all characters are hex digits
        bool isHex = std::all_of(value.begin(), value.end(),
            [](char c) { return std::isxdigit(static_cast<unsigned char>(c)); });

        if (!isHex)
            return std::string(value);

        std::string result;
        result.reserve(value.length() / 2);

        for (size_t i = 0; i < value.length(); i += 2)
        {
            std::string byteStr(value.substr(i, 2));
            try
            {
                int byteValue = std::stoi(byteStr, nullptr, 16);
                result += static_cast<char>(byteValue);
            }
            catch (...)
            {
                return std::string(value); // Return original on error
            }
        }

        return result;
    }
};

/**
 * @brief Built-in Unix timestamp to date conversion plugin
 */
class UnixToDatePlugin : public plugin::IFieldConversionPlugin
{
public:
    std::string GetConversionType() const override { return "unix_to_date"; }
    std::string GetDescription() const override { return "Convert Unix timestamp to human-readable date"; }
    std::string GetConversionName() const override { return "Unix Timestamp to Date"; }

    std::string Convert(std::string_view value, const std::map<std::string, std::string>& /*parameters*/) const override
    {
        try
        {
            time_t timestamp = std::stoll(std::string(value));
            std::tm* tm = std::gmtime(&timestamp);
            if (!tm) return std::string(value);

            char buffer[80];
            std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S UTC", tm);
            return std::string(buffer);
        }
        catch (...)
        {
            return std::string(value);
        }
    }
};

/**
 * @brief Built-in value map conversion plugin
 */
class ValueMapPlugin : public plugin::IFieldConversionPlugin
{
public:
    std::string GetConversionType() const override { return "value_map"; }
    std::string GetDescription() const override { return "Map values using a lookup table"; }
    std::string GetConversionName() const override { return "Value Mapping"; }

    std::string Convert(std::string_view value, const std::map<std::string, std::string>& parameters) const override
    {
        auto it = parameters.find(std::string(value));
        if (it != parameters.end())
        {
            return it->second;
        }
        return std::string(value);
    }
};

/**
 * @brief Built-in ISO Latin-1 to UTF-8 conversion plugin
 */
class IsoLatinPlugin : public plugin::IFieldConversionPlugin
{
public:
    std::string GetConversionType() const override { return "iso_latin"; }
    std::string GetDescription() const override { return "Convert ISO Latin-1 encoded text to UTF-8"; }
    std::string GetConversionName() const override { return "ISO Latin-1 to UTF-8"; }

    std::string Convert(std::string_view value, const std::map<std::string, std::string>& /*parameters*/) const override
    {
        // Simple conversion - in practice you'd use proper encoding libraries
        std::string result;
        result.reserve(value.length());
        for (char c : value)
        {
            unsigned char uc = static_cast<unsigned char>(c);
            if (uc < 128)
            {
                result += c;
            }
            else
            {
                // Basic Latin-1 to UTF-8 (simplified)
                result += static_cast<char>(0xC0 | (uc >> 6));
                result += static_cast<char>(0x80 | (uc & 0x3F));
            }
        }
        return result;
    }
};

/**
 * @brief Built-in NID LRBG conversion plugin
 */
class NidLrbgPlugin : public plugin::IFieldConversionPlugin
{
public:
    std::string GetConversionType() const override { return "nid_lrbg"; }
    std::string GetDescription() const override { return "Convert NID LRBG railway identifier"; }
    std::string GetConversionName() const override { return "NID LRBG Railway ID"; }

    std::string Convert(std::string_view value, const std::map<std::string, std::string>& /*parameters*/) const override
    {
        // Example railway-specific conversion
        if (value.length() >= 8)
        {
            try
            {
                // Extract components from NID LRBG format
                std::string country = std::string(value.substr(0, 2));
                std::string region = std::string(value.substr(2, 4));
                std::string balise = std::string(value.substr(6));

                return "Country: " + country + ", Region: " + region + ", Balise: " + balise;
            }
            catch (...)
            {
                return std::string(value);
            }
        }
        return std::string(value);
    }
};


} // namespace config