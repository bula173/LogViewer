/**
 * @file BasicPlugin.cpp
 * @brief A minimal LogViewer plugin demonstrating the C-ABI plugin interface.
 * 
 * This plugin shows:
 * - Plugin lifecycle (Create, Initialize, Shutdown, Destroy)
 * - Metadata JSON reporting
 * - Logger callback handling
 * - Events container registration
 * 
 * Build with:
 *   cmake -S . -B build
 *   cmake --build build
 * 
 * The plugin will be available in the LogViewer runtime as a dynamic library.
 */

#include <PluginC.h>
#include <PluginEventsC.h>
#include <PluginLoggerC.h>
#include <PluginHostUiC.h>
#include <cstring>
#include <cstdlib>

// Plugin instance state
struct BasicPluginState {
    PluginLogFn logger = nullptr;
    bool initialized = false;
};

// Global logger function (called by the host to provide logging capability)
static PluginLogFn g_logger = nullptr;

// Helper to log messages
static void log_message(int level, const char* msg) {
    if (g_logger) {
        g_logger(level, msg);
    }
}

extern "C" {

/**
 * Plugin_Create: Instantiate the plugin.
 * The host calls this first to create a plugin instance.
 * 
 * @return Opaque plugin handle (allocated by the plugin)
 */
EXPORT_PLUGIN_SYMBOL
PluginHandle Plugin_Create() {
    BasicPluginState* state = new BasicPluginState();
    return reinterpret_cast<PluginHandle>(state);
}

/**
 * Plugin_Destroy: Clean up the plugin instance.
 * Called by the host when unloading the plugin.
 * 
 * @param handle The plugin instance returned by Plugin_Create
 */
EXPORT_PLUGIN_SYMBOL
void Plugin_Destroy(PluginHandle handle) {
    BasicPluginState* state = reinterpret_cast<BasicPluginState*>(handle);
    delete state;
}

/**
 * Plugin_GetApiVersion: Report the plugin API version this plugin was built against.
 * The host uses this to ensure compatibility.
 * 
 * @param handle The plugin instance
 * @return C string with version (e.g., "1.0.0") - NOT freed by plugin
 */
EXPORT_PLUGIN_SYMBOL
const char* Plugin_GetApiVersion(PluginHandle handle) {
    return LOGVIEWER_PLUGIN_API_VERSION;
}

/**
 * Plugin_GetMetadataJson: Return plugin metadata as JSON.
 * The host reads this to learn about the plugin's capabilities.
 * 
 * Example metadata:
 * {
 *   "name": "Basic Example Plugin",
 *   "version": "1.0.0",
 *   "author": "Example Author",
 *   "description": "Demonstrates the LogViewer plugin SDK"
 * }
 * 
 * @param handle The plugin instance
 * @return Malloc'd C string with JSON metadata
 *         MUST be freed by the caller with free()
 */
EXPORT_PLUGIN_SYMBOL
const char* Plugin_GetMetadataJson(PluginHandle handle) {
    const char* json = R"({
        "name": "Basic Example Plugin",
        "version": "1.0.0",
        "author": "Example Author",
        "description": "Minimal plugin demonstrating the LogViewer SDK"
    })";
    
    // Must malloc a copy since the caller will free() it
    size_t len = strlen(json);
    char* copy = (char*)malloc(len + 1);
    strcpy(copy, json);
    return copy;
}

/**
 * Plugin_Initialize: Initialize the plugin.
 * Called after Plugin_Create, with logger callback ready.
 * 
 * @param handle The plugin instance
 * @return true if initialization succeeded, false otherwise
 */
EXPORT_PLUGIN_SYMBOL
bool Plugin_Initialize(PluginHandle handle) {
    BasicPluginState* state = reinterpret_cast<BasicPluginState*>(handle);
    
    log_message(0, "BasicPlugin: Initializing...");
    state->initialized = true;
    log_message(0, "BasicPlugin: Initialized successfully");
    
    return true;
}

/**
 * Plugin_Shutdown: Clean up before destruction.
 * Called before Plugin_Destroy.
 * 
 * @param handle The plugin instance
 */
EXPORT_PLUGIN_SYMBOL
void Plugin_Shutdown(PluginHandle handle) {
    BasicPluginState* state = reinterpret_cast<BasicPluginState*>(handle);
    
    if (state->initialized) {
        log_message(0, "BasicPlugin: Shutting down...");
        state->initialized = false;
    }
}

/**
 * Plugin_GetLastError: Return the last error message.
 * Called if Plugin_Initialize returned false.
 * 
 * @param handle The plugin instance
 * @return Malloc'd C string with error message (freed by caller with free())
 */
EXPORT_PLUGIN_SYMBOL
const char* Plugin_GetLastError(PluginHandle handle) {
    const char* error = "No error";
    size_t len = strlen(error);
    char* copy = (char*)malloc(len + 1);
    strcpy(copy, error);
    return copy;
}

/**
 * Plugin_SetLoggerCallback: Set the logger callback function.
 * The host calls this to provide a logging facility to the plugin.
 * 
 * @param handle The plugin instance
 * @param logger Function pointer to call for logging (level, message)
 */
EXPORT_PLUGIN_SYMBOL
void Plugin_SetLoggerCallback(PluginHandle handle, PluginLogFn logger) {
    g_logger = logger;
    log_message(0, "BasicPlugin: Logger callback registered");
}

} // extern "C"
