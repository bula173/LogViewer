// C-style plugin lifecycle API. Plugins may export these symbols to be loaded
// by the host. Strings are returned as malloc'd `char*` and must be freed
// with `PluginApi_FreeString`.
#pragma once

// Export macro for plugin symbols
#if defined(_WIN32) || defined(_WIN64)
#  ifndef EXPORT_PLUGIN_SYMBOL
#    define EXPORT_PLUGIN_SYMBOL __declspec(dllexport)
#  endif
#else
#  ifndef EXPORT_PLUGIN_SYMBOL
#    define EXPORT_PLUGIN_SYMBOL __attribute__((visibility("default")))
#  endif
#endif

extern "C" {

typedef void* PluginHandle;

// Create/destroy plugin instance. Returns opaque handle or NULL on error.
typedef PluginHandle (*Plugin_Create_Fn)();
typedef void (*Plugin_Destroy_Fn)(PluginHandle);

// Lifecycle
typedef const char* (*Plugin_GetApiVersion_Fn)(PluginHandle);
typedef const char* (*Plugin_GetMetadataJson_Fn)(PluginHandle); // malloc'd c-string
typedef bool (*Plugin_Initialize_Fn)(PluginHandle);
typedef void (*Plugin_Shutdown_Fn)(PluginHandle);
typedef const char* (*Plugin_GetLastError_Fn)(PluginHandle); // malloc'd c-string

// Helper to free strings returned by plugin API functions
inline void PluginApi_FreeString(char* s) { if (s) free(s); }

} // extern "C"
