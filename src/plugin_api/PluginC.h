// C-style plugin lifecycle API. Plugins may export these symbols to be loaded
// by the host. Strings are returned as malloc'd `char*` and must be freed
// with `PluginApi_FreeString`.
#pragma once

#include <stdlib.h>

// Plugin SDK version (merge of PluginSDK.hpp)
#define LOGVIEWER_PLUGIN_API_VERSION "1.0.0"

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

// Generic opaque service handles for optional plugin-provided services
typedef void* PluginServiceHandle;
typedef PluginServiceHandle* PluginServiceArray;

// Create/destroy plugin instance. Returns opaque handle or NULL on error.
typedef PluginHandle (*Plugin_Create_Fn)();
typedef void (*Plugin_Destroy_Fn)(PluginHandle);

// Lifecycle
typedef const char* (*Plugin_GetApiVersion_Fn)(PluginHandle);
typedef const char* (*Plugin_GetMetadataJson_Fn)(PluginHandle); // malloc'd c-string
typedef bool (*Plugin_Initialize_Fn)(PluginHandle);
typedef void (*Plugin_Shutdown_Fn)(PluginHandle);
typedef const char* (*Plugin_GetLastError_Fn)(PluginHandle); // malloc'd c-string

// Logger callback type (from PluginLoggerC.h)
typedef void (*PluginLogFn)(int level, const char* message);

// Set logger callback - called by application after Plugin_Create
typedef void (*Plugin_SetLoggerCallback_Fn)(PluginHandle, PluginLogFn);

// Helper to free strings returned by plugin API functions
inline void PluginApi_FreeString(char* s) { if (s) free(s); }

// Generic panel creation API
// Plugins may export any of these optional C symbols to allow the host to
// embed plugin-provided Qt widgets into the application's UI. Widgets are
// represented as opaque `void*` (QWidget*) across the C ABI. The host will
// pass a parent QWidget* as `parent` so plugins can correctly construct
// child widgets. Returned widget pointers must be heap-allocated Qt widgets
// (e.g. new QWidget(parent)) and may be managed/destroyed by the plugin or
// the host depending on the host's usage. A matching destroy function may be
// provided by the plugin if special teardown is required.

// Signature for creating a panel widget: (pluginHandle, parentWidget, settingsJson) -> QWidget* (opaque)
typedef void* (*Plugin_CreatePanel_Fn)(PluginHandle, void* parentWidget, const char* settingsJson);
// Optional destroy function: (pluginHandle, widget)
typedef void (*Plugin_DestroyPanel_Fn)(PluginHandle, void* widget);

// Well-known export names plugins may provide:
// - Plugin_CreateLeftPanel
// - Plugin_CreateRightPanel
// - Plugin_CreateBottomPanel
// - Plugin_CreateMainPanel


} // extern "C"
