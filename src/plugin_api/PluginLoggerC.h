// C-style logger API for plugins. Header-only; application registers logger callback.
#pragma once

#include <cstdlib>
#include <cstring>

extern "C" {

enum PluginLogLevel {
    PLUGIN_LOG_TRACE = 0,
    PLUGIN_LOG_DEBUG = 1,
    PLUGIN_LOG_INFO  = 2,
    PLUGIN_LOG_WARN  = 3,
    PLUGIN_LOG_ERROR = 4,
    PLUGIN_LOG_CRITICAL = 5
};

typedef void (*PluginLogFn)(int level, const char* message);

// Registration function
void PluginLogger_Register(PluginLogFn fn);

// Helper used by plugins
void PluginLogger_Log(int level, const char* message);

static PluginLogFn g_plugin_logger_fn = nullptr;

inline void PluginLogger_Register(PluginLogFn fn)
{
    g_plugin_logger_fn = fn;
}

inline void PluginLogger_Log(int level, const char* message)
{
    if (g_plugin_logger_fn)
        g_plugin_logger_fn(level, message);
}

} // extern "C"
