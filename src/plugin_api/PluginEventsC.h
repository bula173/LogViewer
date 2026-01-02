// C-style EventsContainer access for plugins.
//
// IMPORTANT: this header is included by both the application and plugin DLLs.
// State must be shared across translation units *within a single binary*.
// Use C++17 inline variables (not TU-local static variables), otherwise
// registering in one .cpp would not be visible to another .cpp (e.g. LogAnalyzer).
#pragma once

#include <cstdlib>

extern "C" {

typedef void* EventsContainerHandle;

// Return number of events
typedef int (*EventsContainer_GetSize_Fn)(EventsContainerHandle handle);
// Return malloc'd JSON string describing event at index
typedef char* (*EventsContainer_GetEventJson_Fn)(EventsContainerHandle handle, int index);

// State shared across translation units in a single binary.
// Note: this does NOT cross DLL boundaries; the host must register callbacks
// within the plugin DLL as well (via a plugin export).
inline EventsContainerHandle g_events_handle = nullptr;
inline EventsContainer_GetSize_Fn g_events_get_size = nullptr;
inline EventsContainer_GetEventJson_Fn g_events_get_event = nullptr;

inline void PluginEvents_Register(EventsContainerHandle handle,
                                  EventsContainer_GetSize_Fn sizeFn,
                                  EventsContainer_GetEventJson_Fn getEventFn)
{
    g_events_handle = handle;
    g_events_get_size = sizeFn;
    g_events_get_event = getEventFn;
}

inline int PluginEvents_GetSize()
{
    if (g_events_handle && g_events_get_size)
        return g_events_get_size(g_events_handle);
    return 0;
}

inline char* PluginEvents_GetEventJson(int index)
{
    if (g_events_handle && g_events_get_event)
        return g_events_get_event(g_events_handle, index);
    return nullptr;
}

} // extern "C"
