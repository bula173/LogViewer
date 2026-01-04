# Plugin System: Type-Specific Interfaces

**⚠️ DEPRECATED DOCUMENT:** This document describes the old `IPlugin` C++ interface, which is **no longer used**. 

## Current Implementation

Modern LogViewer plugins use **C-ABI exports only** (pure C interface). See the following for current documentation:

- **[SDK Getting Started Guide](SDK_GETTING_STARTED.md)** - How to build plugins
- **[SDK Quick Reference](SDK_QUICK_REFERENCE.md)** - API reference
- **[Basic Plugin Example](../examples/BasicPlugin/)** - Working example code
- **[PLUGIN_SYSTEM.md](PLUGIN_SYSTEM.md)** - Architecture overview

## Legacy: Type-Specific Interfaces (Deprecated)

This section is kept for historical reference only. Do not use the patterns described below.

### Old IPlugin Interface

**DEPRECATED - Do not use.** The `IPlugin` C++ interface has been replaced with C-ABI exports:

```cpp
// OLD (Do not use)
class IPlugin {
    virtual bool Initialize() = 0;
    virtual void Shutdown() = 0;
    // ... other virtual methods
};

// NEW (Use this)
extern "C" {
    EXPORT_PLUGIN_SYMBOL
    PluginHandle Plugin_Create() { ... }
    
    EXPORT_PLUGIN_SYMBOL
    bool Plugin_Initialize(PluginHandle h) { ... }
    // ... other C-ABI exports
}
```

### Migration to C-ABI

If you have an old plugin using `IPlugin`, migrate it to use C-ABI:

1. Remove `IPlugin` inheritance
2. Export C-ABI functions (see SDK guides)
3. Store plugin state in opaque handle
4. Use `find_package(LogViewer CONFIG REQUIRED)` in CMakeLists.txt
5. Follow the [Basic Plugin Example](../examples/BasicPlugin/) pattern

## Summary

✅ **Current**: C-ABI plugin interface (stable, compiler-agnostic)  
❌ **Deprecated**: C++ IPlugin interface (no longer supported)
