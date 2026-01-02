#include "PluginLoggerC.h"

static PluginLogFn g_plugin_logger_fn = nullptr;

extern "C" {

void PluginLogger_Register(PluginLogFn fn)
{
    g_plugin_logger_fn = fn;
}

void PluginLogger_Log(int level, const char* message)
{
    if (g_plugin_logger_fn && message)
        g_plugin_logger_fn(level, message);
}

} // extern "C"
