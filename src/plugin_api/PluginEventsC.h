// C-style EventsContainer access for plugins. Application registers callbacks
// to provide access to its EventsContainer without exposing C++ types.
#pragma once

#include <cstdlib>

extern "C" {

typedef void* EventsContainerHandle;

// Return number of events
typedef int (*EventsContainer_GetSize_Fn)(EventsContainerHandle handle);
// Return malloc'd JSON string describing event at index
typedef char* (*EventsContainer_GetEventJson_Fn)(EventsContainerHandle handle, int index);

// Registration performed by application
void PluginEvents_Register(EventsContainerHandle handle,
                           EventsContainer_GetSize_Fn sizeFn,
                           EventsContainer_GetEventJson_Fn getEventFn);

// Helpers used by plugins
int PluginEvents_GetSize();
char* PluginEvents_GetEventJson(int index); // returns malloc'd string or nullptr

static EventsContainerHandle g_events_handle = nullptr;
static EventsContainer_GetSize_Fn g_events_get_size = nullptr;
static EventsContainer_GetEventJson_Fn g_events_get_event = nullptr;

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
    if (g_events_handle && g_events_get_size) return g_events_get_size(g_events_handle);
    return 0;
}

inline char* PluginEvents_GetEventJson(int index)
{
    if (g_events_handle && g_events_get_event) return g_events_get_event(g_events_handle, index);
    return nullptr;
}

} // extern "C"
