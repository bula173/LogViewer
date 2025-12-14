#include "plugins/IFieldConversionPlugin.hpp"
#include "config/FieldConversionPluginRegistry.hpp"
#include <algorithm>

namespace config
{

/**
 * @brief Example plugin that reverses strings
 */
class StringReversePlugin : public plugin::IFieldConversionPlugin
{
public:
    std::string GetConversionType() const override { return "string_reverse"; }
    std::string GetDescription() const override { return "Reverse the characters in a string"; }
    std::string GetConversionName() const override { return "String Reverse"; }

    std::string Convert(std::string_view value, const std::map<std::string, std::string>& /*parameters*/) const override
    {
        std::string result(value);
        std::reverse(result.begin(), result.end());
        return result;
    }
};

// Registration function for this plugin
void RegisterStringReversePlugin()
{
    auto& registry = FieldConversionPluginRegistry::GetInstance();
    registry.RegisterPlugin([]() { return std::make_unique<StringReversePlugin>(); });
}

} // namespace config