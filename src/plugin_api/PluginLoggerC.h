// C-style logger API for plugins. The application passes a callback to the plugin.
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

enum PluginLogLevel {
    PLUGIN_LOG_TRACE = 0,
    PLUGIN_LOG_DEBUG = 1,
    PLUGIN_LOG_INFO  = 2,
    PLUGIN_LOG_WARN  = 3,
    PLUGIN_LOG_ERROR = 4,
    PLUGIN_LOG_CRITICAL = 5
};

typedef void (*PluginLogFn)(int level, const char* message);

// Global logger callback - set by Plugin_SetLoggerCallback C-ABI function
static PluginLogFn g_plugin_logger_fn = nullptr;

// Registration function - called by application to set the logging callback
// This function is provided for backward compatibility and internal use
inline void PluginLogger_Register(PluginLogFn fn)
{
    g_plugin_logger_fn = fn;
}

// Logging function - called by plugins to log messages
inline void PluginLogger_Log(int level, const char* message)
{
    if (g_plugin_logger_fn && message) {
        g_plugin_logger_fn(level, message);
    }
}

#ifdef __cplusplus
}
#endif
