# Plugin Observer Pattern

**⚠️ PARTIALLY DEPRECATED:** This document describes the observer pattern for plugin lifecycle events, which is still used internally. However, the `IPlugin` C++ interface is **no longer used**. Modern plugins use **C-ABI exports only** (see [PLUGIN_SYSTEM.md](PLUGIN_SYSTEM.md)).

## Overview

The PluginManager uses the Observer pattern to notify interested parties about plugin lifecycle events. This allows the UI and other components to react to plugin loading, enabling, disabling, and unloading.

## Architecture

### IPluginObserver Interface

**Note:** While the observer pattern is still used, observers now receive `PluginHandle` (opaque pointer) instead of `IPlugin*` since plugins use C-ABI.

```cpp
class IPluginObserver
{
public:
    virtual ~IPluginObserver() = default;
    
    virtual void OnPluginEvent(PluginEvent event, 
                              const std::string& pluginId,
                              void* pluginHandle) = 0;  // Changed from IPlugin*
};
```

### Plugin Events

```cpp
enum class PluginEvent
{
    Loaded,      // Plugin was loaded into memory
    Unloaded,    // Plugin was unloaded from memory
    Enabled,     // Plugin was enabled (activated)
    Disabled,    // Plugin was disabled (deactivated)
    Registered,  // Plugin was registered (file copied to user dir)
    Unregistered // Plugin was unregistered (file removed from user dir)
};
```

## Event Flow

### Registration & Loading
```
1. RegisterPlugin() → copies file to user directory
2. Fires: PluginEvent::Registered
3. Loads plugin into memory
4. Fires: PluginEvent::Loaded
5. Marks as enabled
6. Fires: PluginEvent::Enabled
```

### Enable/Disable Cycle
```
Enable:
1. EnablePlugin() → checks if plugin in memory
2. If not loaded: Load → PluginEvent::Loaded
3. Set enabled=true
4. Fire: PluginEvent::Enabled → UI creates tab

Disable:
1. DisablePlugin() → checks if enabled
2. Set enabled=false
3. Fire: PluginEvent::Disabled → UI removes tab
4. Plugin stays in memory for quick re-enable
```

### Unregistration
```
1. UnregisterPlugin() → disables if enabled
2. Fires: PluginEvent::Disabled (if was enabled)
3. Deletes file from user directory
4. Fires: PluginEvent::Unregistered
5. Unloads from memory
6. Fires: PluginEvent::Unloaded
```

## Implementation Example

### Register as Observer

```cpp
class MainWindow : public QMainWindow,
                   public plugin::IPluginObserver
{
public:
    void setupPluginManager() {
        auto& pluginManager = plugin::PluginManager::GetInstance();
        pluginManager.RegisterObserver(this);
    }
    
    void OnPluginEvent(plugin::PluginEvent event, 
                      const std::string& pluginId,
                      void* pluginHandle) override {  // Changed from IPlugin*
        switch (event) {
            case PluginEvent::Enabled:
                createPluginTab(pluginId, pluginHandle);
                break;
            case PluginEvent::Disabled:
                removePluginTab(pluginId);
                break;
            case PluginEvent::Registered:
                // Update plugin list UI
                break;
            // ... handle other events
        }
    }
};
```

### Multiple Observers

The observer pattern allows multiple components to react to plugin events:

```cpp
// UI updates
mainWindow->RegisterObserver(pluginManager);

// Logging
pluginLogger->RegisterObserver(pluginManager);

// Analytics
analyticsTracker->RegisterObserver(pluginManager);

// Configuration persistence
configManager->RegisterObserver(pluginManager);
```

## Benefits

1. **Decoupling**: PluginManager doesn't need to know about UI specifics
2. **Extensibility**: Easy to add new observers without modifying PluginManager
3. **Multiple Observers**: Many components can react to the same events
4. **Clear Event Types**: Explicit enum makes event handling more maintainable
5. **Single Notification Point**: All observers notified in consistent order

## Migration from Callbacks

### Old Approach
```cpp
pluginManager.SetPluginEnableCallback([](id, plugin) { /* ... */ });
pluginManager.SetPluginDisableCallback([](id) { /* ... */ });
pluginManager.SetPluginLoadCallback([](id, plugin) { /* ... */ });
pluginManager.SetPluginUnloadCallback([](id) { /* ... */ });
```

### New Approach
```cpp
pluginManager.RegisterObserver(observer);
// Observer receives all events through single OnPluginEvent() method
```

## Event Timing

| Event | When | Plugin Pointer |
|-------|------|----------------|
| Registered | After file copied | Valid |
| Loaded | After library loaded | Valid |
| Enabled | After marking enabled | Valid |
| Disabled | Before marking disabled | Valid |
| Unloaded | Before unloading | nullptr |
| Unregistered | After file deleted | nullptr |

## Thread Safety

- All observer notifications happen on the same thread that calls PluginManager methods
- Observers should not perform long-running operations in OnPluginEvent()
- UI updates from observers should be queued to the main thread if needed

## Best Practices

1. **Unregister on Destruction**: Always unregister observers before they're destroyed
   ```cpp
   ~MainWindow() {
       plugin::PluginManager::GetInstance().UnregisterObserver(this);
   }
   ```

2. **Handle All Events**: Use switch statement with all cases or default
   ```cpp
   void OnPluginEvent(PluginEvent event, ...) {
       switch (event) {
           case PluginEvent::Enabled: /* ... */ break;
           case PluginEvent::Disabled: /* ... */ break;
           // ... all other events
           default:
               // Future-proof for new event types
               break;
       }
   }
   ```

3. **Quick Processing**: Keep event handlers fast
   ```cpp
   void OnPluginEvent(PluginEvent event, ...) {
       // Good: Schedule work for later
       QMetaObject::invokeMethod(this, [this, pluginId]() {
           // Heavy work here
       }, Qt::QueuedConnection);
   }
   ```

4. **Null Checks**: Always check plugin pointer for Unloaded/Unregistered events
   ```cpp
   void OnPluginEvent(PluginEvent event, const std::string& id, IPlugin* plugin) {
       if (event == PluginEvent::Unloaded && plugin == nullptr) {
           // Expected - plugin no longer available
       }
   }
   ```
