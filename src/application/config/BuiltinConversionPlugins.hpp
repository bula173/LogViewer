#include "IFieldConversionPlugin.hpp"
#include "FieldConversionPluginRegistry.hpp"
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
            const std::tm* tm = std::gmtime(&timestamp);
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
        // ERTMS NID_LRBG: 24-bit identifier
        // NID_C (country): bits 23-14 (10 bits)
        // NID_BG (balise group): bits 13-0 (14 bits)
        
        if (value.empty())
            return std::string(value);

        try
        {
            // Parse NID_LRBG as integer (supports decimal and hex with 0x prefix)
            unsigned long nid_lrbg;
            if (value.length() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X'))
            {
                nid_lrbg = std::stoul(std::string(value), nullptr, 16);
            }
            else
            {
                nid_lrbg = std::stoul(std::string(value), nullptr, 10);
            }

            // Ensure it's a valid 24-bit value
            if (nid_lrbg > 0xFFFFFF)
                return std::string(value);

            // Extract NID_C (upper 10 bits) and NID_BG (lower 14 bits)
            unsigned int nid_c = (nid_lrbg >> 14) & 0x3FF;
            unsigned int nid_bg = nid_lrbg & 0x3FFF;

            std::ostringstream oss;
            oss << "nid_c: " << nid_c << ", nid_bg: " << nid_bg;
            return oss.str();
        }
        catch (...)
        {
            return std::string(value);
        }
    }
};

/**
 * @brief Built-in ERTMS NID_ENGINE conversion plugin
 * Extracts M_VOLTAGE (3 bits) and NID_ENGINE (21 bits) from 24-bit engine identifier
 */
class NidEnginePlugin : public plugin::IFieldConversionPlugin
{
public:
    std::string GetConversionType() const override { return "nid_engine"; }
    std::string GetDescription() const override { return "Extract M_VOLTAGE and NID_ENGINE from 24-bit engine identifier"; }
    std::string GetConversionName() const override { return "ERTMS Engine ID"; }

    std::string Convert(std::string_view value, const std::map<std::string, std::string>& /*parameters*/) const override
    {
        if (value.empty())
            return std::string(value);

        try
        {
            unsigned long nid_engine_full;
            if (value.length() > 2 && value[0] == '0' && (value[1] == 'x' || value[1] == 'X'))
            {
                nid_engine_full = std::stoul(std::string(value), nullptr, 16);
            }
            else
            {
                nid_engine_full = std::stoul(std::string(value), nullptr, 10);
            }

            if (nid_engine_full > 0xFFFFFF)
                return std::string(value);

            // M_VOLTAGE: bits 23-21 (3 bits)
            // NID_ENGINE: bits 20-0 (21 bits)
            unsigned int m_voltage = (nid_engine_full >> 21) & 0x7;
            unsigned int nid_engine = nid_engine_full & 0x1FFFFF;

            const char* voltage_str;
            switch (m_voltage)
            {
                case 0: voltage_str = "Line not fitted"; break;
                case 1: voltage_str = "AC 25kV 50Hz"; break;
                case 2: voltage_str = "AC 15kV 16.7Hz"; break;
                case 3: voltage_str = "DC 3kV"; break;
                case 4: voltage_str = "DC 1.5kV"; break;
                case 5: voltage_str = "DC 600/750V"; break;
                default: voltage_str = "Reserved"; break;
            }

            std::ostringstream oss;
            oss << "voltage: " << voltage_str << ", engine: " << nid_engine;
            return oss.str();
        }
        catch (...)
        {
            return std::string(value);
        }
    }
};

/**
 * @brief Built-in ERTMS Track Condition conversion plugin
 * Decodes M_TRACKCOND (4 bits) from packet 68
 */
class TrackConditionPlugin : public plugin::IFieldConversionPlugin
{
public:
    std::string GetConversionType() const override { return "m_trackcond"; }
    std::string GetDescription() const override { return "Decode ERTMS track condition type"; }
    std::string GetConversionName() const override { return "ERTMS Track Condition"; }

    std::string Convert(std::string_view value, const std::map<std::string, std::string>& /*parameters*/) const override
    {
        try
        {
            int condition = std::stoi(std::string(value));
            switch (condition)
            {
                case 0: return "Non stopping area (packet 68.0)";
                case 1: return "Tunnel stopping area (packet 68.1)";
                case 2: return "Sound horn (packet 68.2)";
                case 3: return "Powerless section - lower pantograph (packet 68.3)";
                case 4: return "Radio hole (packet 68.4)";
                case 5: return "Air tightness (packet 68.5)";
                case 6: return "Switch off regenerative brake (packet 68.6)";
                case 7: return "Switch off eddy current brake (packet 68.7)";
                case 8: return "Switch off magnetic shoe brake (packet 68.8)";
                case 9: return "Powerless section - switch off main power switch (packet 68.9)";
                case 10: return "Change of traction system (packet 68.10)";
                case 11: return "Change of allowed current consumption (packet 68.11)";
                default: return "Unknown condition (" + std::string(value) + ")";
            }
        }
        catch (...)
        {
            return std::string(value);
        }
    }
};

/**
 * @brief Built-in ERTMS Position Report Parameters plugin
 * Decodes T_CYCLOC (8 bits) cycle time into seconds
 */
class TCyclocPlugin : public plugin::IFieldConversionPlugin
{
public:
    std::string GetConversionType() const override { return "t_cycloc"; }
    std::string GetDescription() const override { return "Convert T_CYCLOC position report cycle time"; }
    std::string GetConversionName() const override { return "Position Report Cycle"; }

    std::string Convert(std::string_view value, const std::map<std::string, std::string>& /*parameters*/) const override
    {
        try
        {
            int t_cycloc = std::stoi(std::string(value));
            
            // T_CYCLOC = 0: No regular position reports
            if (t_cycloc == 0)
                return "No cyclic reporting";
            
            // T_CYCLOC > 0: (value * 10) seconds
            int seconds = t_cycloc * 10;
            
            std::ostringstream oss;
            if (seconds >= 60)
            {
                int minutes = seconds / 60;
                int remaining_seconds = seconds % 60;
                oss << minutes << "min";
                if (remaining_seconds > 0)
                    oss << " " << remaining_seconds << "s";
            }
            else
            {
                oss << seconds << "s";
            }
            
            oss << " (" << t_cycloc << ")";
            return oss.str();
        }
        catch (...)
        {
            return std::string(value);
        }
    }
};


} // namespace config