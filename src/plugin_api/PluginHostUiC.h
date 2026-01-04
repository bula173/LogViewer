#pragma once

// Minimal host->plugin callback interface for UI interactions.
// This is OPTIONAL: plugins may export Plugin_SetHostUiCallbacks to receive it.

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*PluginHost_SetCurrentItem_Fn)(void* hostOpaque, int itemIndex);

typedef struct PluginHostUiCallbacks {
    uint32_t size; // sizeof(PluginHostUiCallbacks)
    PluginHost_SetCurrentItem_Fn setCurrentItem;
} PluginHostUiCallbacks;

#ifdef __cplusplus
} // extern "C"
#endif
