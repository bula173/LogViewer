// IFieldConversionPlugin internal interface
#pragma once

#include <string>
#include <string_view>
#include <map>

namespace plugin {

class IFieldConversionPlugin
{
public:
    virtual ~IFieldConversionPlugin() = default;
    virtual std::string GetConversionType() const = 0;
    virtual std::string GetDescription() const = 0;
    virtual std::string Convert(std::string_view value, const std::map<std::string, std::string>& parameters) const = 0;
    virtual bool CanConvert(std::string_view value) const { (void)value; return true; }
    virtual std::string GetConversionName() const = 0;
};

} // namespace plugin
