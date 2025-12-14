/**
 * @file IFieldConversionPlugin.hpp
 * @brief Field conversion plugin interface for custom field transformations
 * @author LogViewer Development Team
 * @date 2025
 */

#ifndef PLUGIN_IFIELDCONVERSIONPLUGIN_HPP
#define PLUGIN_IFIELDCONVERSIONPLUGIN_HPP

#include <string>
#include <string_view>
#include <map>

namespace plugin
{

/**
 * @class IFieldConversionPlugin
 * @brief Interface for plugins that provide custom field value conversions
 * 
 * This interface allows plugins to register custom conversion types that can be
 * applied to field values in the log viewer (e.g., hex to ASCII, base64 decode,
 * timestamp formatting, etc.).
 */
class IFieldConversionPlugin
{
public:
    /**
     * @brief Virtual destructor
     */
    virtual ~IFieldConversionPlugin() = default;

    /**
     * @brief Gets the unique identifier for this conversion type
     * @return Conversion type identifier (e.g., "hex_to_ascii", "base64_decode")
     * 
     * This identifier is used in configuration files and UI to reference
     * this specific conversion.
     */
    virtual std::string GetConversionType() const = 0;

    /**
     * @brief Gets a human-readable description of this conversion
     * @return Description string for UI display
     */
    virtual std::string GetDescription() const = 0;

    /**
     * @brief Applies the conversion to a field value
     * @param value The original field value to convert
     * @param parameters Additional parameters from configuration (optional)
     * @return The converted value, or original value if conversion fails
     * 
     * This method should be thread-safe as it may be called from multiple
     * threads simultaneously.
     */
    virtual std::string Convert(std::string_view value, 
                                const std::map<std::string, std::string>& parameters) const = 0;

    /**
     * @brief Checks if this conversion can handle the given value
     * @param value The value to check
     * @return true if the conversion can be applied, false otherwise
     * 
     * This allows plugins to validate input before attempting conversion.
     * Default implementation returns true (accepts all values).
     */
    virtual bool CanConvert(std::string_view value) const { 
        (void)value;
        return true; 
    }

    /**
     * @brief Gets the name for UI display
     * @return Human-readable name for the conversion
     */
    virtual std::string GetConversionName() const = 0;
};

} // namespace plugin

#endif // PLUGIN_IFIELDCONVERSIONPLUGIN_HPP
