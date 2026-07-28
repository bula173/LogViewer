// C-ABI for "source" plugins that inject events into the host.
//
// A source plugin (SSH streamer, serial port, named pipe, etc.) receives live
// data from an external system and converts it into LogViewer events. Because
// the plugin runs inside the host process the host must provide a callback so
// the plugin can append events without depending on internal C++ types.
//
// Usage (host side):
//   PluginSourceCallbacks cbs{};
//   cbs.size       = sizeof(PluginSourceCallbacks);
//   cbs.hostOpaque = myEventsContainer;
//   cbs.appendEvent = [](void* opaque, const char* json) { /* parse + append */ };
//   cbs.clearEvents = [](void* opaque) { /* clear container */ };
//   Plugin_SetSourceCallbacks_Fn fn = dlsym(lib, "Plugin_SetSourceCallbacks");
//   if (fn) fn(handle, &cbs);
//
// Event JSON format (produced by source plugins):
//   {
//     "timestamp" : "<ISO-8601 or raw string>",
//     "level"     : "INFO|WARN|ERROR|DEBUG|TRACE|CRITICAL",
//     "actor"     : "<process / service name>",
//     "message"   : "<human-readable text>",
//     "source"    : "<plugin-specific origin, e.g. user@host:22>",
//     "extra"     : { "<key>": "<value>", ... }   // optional named capture groups
//   }
#pragma once

#include "PluginC.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Callback the host registers so the plugin can push new events.
typedef void (*PluginHost_AppendEventJson_Fn)(void* hostOpaque, const char* eventJson);

// Callback the host registers so the plugin can request a full clear.
typedef void (*PluginHost_ClearEvents_Fn)(void* hostOpaque);

// Versioned struct passed from host to plugin via Plugin_SetSourceCallbacks.
typedef struct PluginSourceCallbacks {
    uint32_t size;                              // sizeof(PluginSourceCallbacks)
    void*    hostOpaque;                        // opaque host context (e.g. EventsContainer*)
    PluginHost_AppendEventJson_Fn appendEvent;  // non-null: add a parsed event
    PluginHost_ClearEvents_Fn     clearEvents;  // may be null: clear all events
} PluginSourceCallbacks;

// C-ABI function type plugins must export as "Plugin_SetSourceCallbacks"
typedef void (*Plugin_SetSourceCallbacks_Fn)(PluginHandle, const PluginSourceCallbacks*);

// C-ABI function type plugins may export as "Plugin_Connect" / "Plugin_Disconnect"
// settingsJson: JSON object with connection parameters (host, user, port, command, …)
typedef bool (*Plugin_Connect_Fn)(PluginHandle, const char* settingsJson);
typedef void (*Plugin_Disconnect_Fn)(PluginHandle);

#ifdef __cplusplus
} // extern "C"
#endif
