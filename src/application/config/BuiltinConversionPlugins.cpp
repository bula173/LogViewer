#include "config/BuiltinConversionPlugins.hpp"
#include <algorithm>

namespace config
{
/**
 * @brief Register all built-in conversion plugins
 */
void RegisterBuiltinConversionPlugins()
{
    auto& registry = FieldConversionPluginRegistry::GetInstance();

    registry.RegisterPlugin([]() { return std::make_unique<HexToAsciiPlugin>(); });
    registry.RegisterPlugin([]() { return std::make_unique<UnixToDatePlugin>(); });
    registry.RegisterPlugin([]() { return std::make_unique<ValueMapPlugin>(); });
    registry.RegisterPlugin([]() { return std::make_unique<IsoLatinPlugin>(); });
    registry.RegisterPlugin([]() { return std::make_unique<NidLrbgPlugin>(); });
}
} // namespace config