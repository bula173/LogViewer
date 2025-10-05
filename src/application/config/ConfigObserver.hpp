#pragma once

namespace config
{

/**
 * Observer interface for configuration changes
 */
class ConfigObserver
{
  public:
    virtual ~ConfigObserver() = default;

    /**
     * Called when configuration has been changed and saved
     */
    virtual void OnConfigChanged() = 0;
};

} // namespace config