/**
 * @file PluginEventBus.hpp
 * @brief Enhanced plugin event system with fine-grained lifecycle notifications
 * @author LogViewer Development Team
 * @date 2026
 */

#ifndef PLUGIN_EVENT_BUS_HPP
#define PLUGIN_EVENT_BUS_HPP

#include "Logger.hpp"
#include <chrono>
#include <memory>
#include <shared_mutex>
#include <string>
#include <vector>

namespace plugin {

/**
 * @enum PluginEventType
 * @brief Fine-grained plugin lifecycle events
 *
 * Provides comprehensive event notifications for plugin state changes,
 * enabling listeners to react to initialization, configuration, and error states.
 */
enum class PluginEventType {
    /// Basic lifecycle events
    Loaded,     ///< Plugin binary loaded from disk
    Unloaded,   ///< Plugin binary unloaded from memory

    /// Initialization lifecycle
    InitializationStarted,   ///< Plugin Initialize() called
    InitializationCompleted, ///< Plugin Initialize() succeeded
    InitializationFailed,    ///< Plugin Initialize() failed

    /// State change events
    Enabled,  ///< Plugin enabled/activated
    Disabled, ///< Plugin disabled/deactivated

    /// Configuration events
    ConfigurationChanged, ///< Plugin configuration modified

    /// Resource lifecycle
    PanelCreated,   ///< UI panel created
    PanelDestroyed, ///< UI panel destroyed

    /// Error events
    ErrorOccurred, ///< Plugin encountered an error
};

/**
 * @struct PluginEvent
 * @brief Detailed information about plugin state change
 */
struct PluginEvent {
    PluginEventType type;                        ///< Event type
    std::string pluginId;                        ///< Plugin ID
    void* plugin;                                ///< Plugin instance (nullptr for Unloaded)
    std::string details;                         ///< Additional details (error msg, config key)
    std::chrono::system_clock::time_point timestamp; ///< Event timestamp
};

/**
 * @class IPluginEventObserver
 * @brief Interface for observing plugin events
 *
 * Implement this interface to receive notifications of plugin state changes.
 */
class IPluginEventObserver {
  public:
    virtual ~IPluginEventObserver() = default;

    /**
     * @brief Called when a plugin event occurs
     * @param event The plugin event details
     *
     * Implementations should not throw exceptions.
     */
    virtual void OnPluginEvent(const PluginEvent& event) = 0;
};

/**
 * @class PluginEventBus
 * @brief Central event bus for plugin lifecycle notifications
 *
 * Manages subscriptions to plugin events and notifies all interested
 * observers when events occur. Uses weak_ptr to automatically clean up
 * destroyed observers.
 *
 * @par Thread Safety
 * Thread-safe with shared_mutex for concurrent observer access.
 *
 * @par Usage Example
 * @code
 * class MyObserver : public IPluginEventObserver {
 *     void OnPluginEvent(const PluginEvent& event) override {
 *         if (event.type == PluginEventType::Loaded) {
 *             Logger::Info("Plugin loaded: {}", event.pluginId);
 *         }
 *     }
 * };
 *
 * auto busextra = PluginEventBus::GetInstance();
 * auto observer = std::make_shared<MyObserver>();
 * bus.Subscribe(observer);
 *
 * // Later, plugins or system publishes events
 * PluginEvent evt{PluginEventType::Loaded, "plugin-id", ...};
 * bus.PublishEvent(evt);
 * @endcode
 */
class PluginEventBus {
  public:
    /// @brief Shared pointer to observer
    using ObserverPtr = std::shared_ptr<IPluginEventObserver>;

    /**
     * @brief Gets singleton instance
     * @return Reference to PluginEventBus singleton
     */
    static PluginEventBus& GetInstance()
    {
        static PluginEventBus instance;
        return instance;
    }

    // Prevent copying
    PluginEventBus(const PluginEventBus&) = delete;
    PluginEventBus& operator=(const PluginEventBus&) = delete;

    /**
     * @brief Subscribes observer to plugin events
     *
     * @param observer Shared pointer to observer
     *
     * Observer will receive OnPluginEvent callbacks until destroyed
     * or explicitly unsubscribed.
     */
    void Subscribe(ObserverPtr observer)
    {
        if (!observer) {
            util::Logger::Warn("Attempt to subscribe null observer");
            return;
        }

        std::unique_lock lock(m_observersMutex);
        m_observers.push_back(std::weak_ptr<IPluginEventObserver>(observer));

        util::Logger::Trace("Plugin event observer subscribed, total: {}",
                           m_observers.size());
    }

    /**
     * @brief Unsubscribes observer from plugin events
     *
     * @param observer Observer to unsubscribe
     *
     * Safe if observer not subscribed or already destroyed.
     */
    void Unsubscribe(const IPluginEventObserver* observer)
    {
        if (!observer) {
            return;
        }

        std::unique_lock lock(m_observersMutex);
        m_observers.erase(
            std::remove_if(m_observers.begin(), m_observers.end(),
                           [observer](const auto& weak) {
                               auto shared = weak.lock();
                               return !shared || shared.get() == observer;
                           }),
            m_observers.end());

        util::Logger::Trace("Plugin event observer unsubscribed");
    }

    /**
     * @brief Publishes an event to all subscribed observers
     *
     * @param event The plugin event to publish
     *
     * Dead observers are automatically detected and removed.
     * If an observer throws, the exception is logged but other observers
     * are still notified (best-effort delivery).
     */
    void PublishEvent(const PluginEvent& event)
    {
        std::shared_lock lock(m_observersMutex);

        // Log event for debugging
        LogEvent(event);

        // Notify all live observers
        for (auto& weak : m_observers) {
            if (auto observer = weak.lock()) {
                try {
                    observer->OnPluginEvent(event);
                } catch (const std::exception& e) {
                    util::Logger::Error("Observer exception: {}", e.what());
                }
            }
        }

        // Cleanup dead observers on each publish
        CleanupDeadObserversUnlocked();
    }

    /**
     * @brief Gets number of active observers
     * @return Count of subscribed observers
     */
    size_t GetObserverCount() const
    {
        std::shared_lock lock(m_observersMutex);
        return m_observers.size();
    }

  private:
    PluginEventBus() = default;

    mutable std::shared_mutex m_observersMutex;
    std::vector<std::weak_ptr<IPluginEventObserver>> m_observers;

    /**
     * @brief Logs event details for debugging
     *
     * @param event Event to log
     */
    void LogEvent(const PluginEvent& event) const
    {
        const char* eventTypeStr = [](PluginEventType type) {
            switch (type) {
                case PluginEventType::Loaded:
                    return "Loaded";
                case PluginEventType::Unloaded:
                    return "Unloaded";
                case PluginEventType::InitializationStarted:
                    return "InitStart";
                case PluginEventType::InitializationCompleted:
                    return "InitDone";
                case PluginEventType::InitializationFailed:
                    return "InitFailed";
                case PluginEventType::Enabled:
                    return "Enabled";
                case PluginEventType::Disabled:
                    return "Disabled";
                case PluginEventType::ConfigurationChanged:
                    return "ConfigChanged";
                case PluginEventType::PanelCreated:
                    return "PanelCreated";
                case PluginEventType::PanelDestroyed:
                    return "PanelDestroyed";
                case PluginEventType::ErrorOccurred:
                    return "Error";
                default:
                    return "Unknown";
            }
        }(event.type);

        if (event.details.empty()) {
            util::Logger::Trace("Plugin event: {} [{}]", eventTypeStr,
                               event.pluginId);
        } else {
            util::Logger::Trace("Plugin event: {} [{}] details: {}",
                               eventTypeStr, event.pluginId, event.details);
        }
    }

    /**
     * @brief Removes expired weak_ptr observers
     *
     * Called with lock held to clean up dead observers.
     */
    void CleanupDeadObserversUnlocked()
    {
        auto erased = std::remove_if(
            m_observers.begin(), m_observers.end(),
            [](const auto& weak) { return weak.expired(); });

        size_t count = std::distance(erased, m_observers.end());
        if (count > 0) {
            util::Logger::Trace("Cleaned up {} expired event observers",
                               count);
            m_observers.erase(erased, m_observers.end());
        }
    }
};

} // namespace plugin

#endif // PLUGIN_EVENT_BUS_HPP
