# AI Provider Plugin

## Overview

The AI Provider Plugin (`ai_provider`) is a C-ABI plugin that adds AI-powered log analysis capabilities to LogViewer. It supports multiple AI providers (local and cloud) and provides three UI panels for interaction.

## Architecture

### C-ABI Exports

The plugin exports the following C functions:

**Lifecycle:**
- `Plugin_Create()` - Creates plugin instance
- `Plugin_Destroy(handle)` - Destroys plugin instance
- `Plugin_SetLoggerCallback(handle, logFn)` - Receives logging callback from application
- `Plugin_GetMetadataJson(handle)` - Returns plugin metadata as JSON
- `Plugin_Initialize(handle)` - Initializes plugin resources
- `Plugin_Shutdown(handle)` - Cleans up plugin resources

**UI Panels:**
- `Plugin_CreateMainPanel(handle, parent, settings)` - Creates AI analysis panel
- `Plugin_CreateBottomPanel(handle, parent, settings)` - Creates chat panel
- `Plugin_CreateLeftPanel(handle, parent, settings)` - Creates configuration panel

**AI Service:**
- `Plugin_CreateAIService(handle, settings)` - Creates AI service instance
- `Plugin_DestroyAIService(serviceHandle)` - Destroys AI service
- `Plugin_SetAIEventsContainer(handle, events)` - Provides log events to plugin

## UI Panels

### 1. Configuration Panel (Left Dock)

**Location:** Left dock area, separate tab alongside "Filters"

**Purpose:** Configure AI provider settings

**Features:**
- AI provider selection (Ollama, OpenAI, Anthropic, Google Gemini, xAI Grok, LM Studio)
- API key management (encrypted storage)
- Model selection
- Base URL configuration (for Ollama/LM Studio)
- Connection testing

**Implementation:**
```cpp
QWidget* Plugin_CreateLeftPanel(PluginHandle h, void* parent, const char* settings) {
    auto* plugin = reinterpret_cast<AIProviderPlugin*>(h);
    QWidget* parentWidget = reinterpret_cast<QWidget*>(parent);
    return plugin->GetConfigurationUI(parentWidget);
}
```

**Created by:** `AIConfigPanel` class with parent widget properly set

### 2. Analysis Panel (Main Content)

**Location:** Main content area, tab "AI Provider"

**Purpose:** Perform analysis on log entries

**Features:**
- Analysis type selection (Summary, Error Analysis, Pattern Detection, etc.)
- Max events slider (limit analysis scope)
- Custom prompt input
- Analysis results display
- Save/load prompts

**Implementation:**
```cpp
QWidget* Plugin_CreateMainPanel(PluginHandle h, void* parent, const char* settings) {
    auto* plugin = reinterpret_cast<AIProviderPlugin*>(h);
    QWidget* parentWidget = reinterpret_cast<QWidget*>(parent);
    return plugin->CreateTab(parentWidget);
}
```

**Created by:** `AIAnalysisPanel` class

### 3. Chat Panel (Bottom Dock)

**Location:** Bottom dock area, tab "AI Provider Chat"

**Purpose:** Interactive chat with AI about logs

**Features:**
- Chat history display
- Message input
- Context-aware responses (AI sees filtered log events)
- Clear chat button

**Implementation:**
```cpp
QWidget* Plugin_CreateBottomPanel(PluginHandle h, void* parent, const char* settings) {
    auto* plugin = reinterpret_cast<AIProviderPlugin*>(h);
    QWidget* parentWidget = reinterpret_cast<QWidget*>(parent);
    return plugin->CreateBottomPanel(parentWidget, nullptr, nullptr);
}
```

**Created by:** `AIChatPanel` class

## Logging

The plugin uses the application's logging system through a callback mechanism:

```cpp
void Plugin_SetLoggerCallback(PluginHandle h, PluginLogFn logFn) {
    // Store the logging callback
    PluginLogger_Register(logFn);
}
```

**Usage in plugin:**
```cpp
PluginLogger_Log(PLUGIN_LOG_INFO, "Creating AI service");
PluginLogger_Log(PLUGIN_LOG_ERROR, "Failed to connect to provider");
PluginLogger_Log(PLUGIN_LOG_WARN, "API key not configured");
```

**Log output:**
```
[2026-01-02 12:00:00.123] [LogViewer] [info] [plugin] Creating AI service
[2026-01-02 12:00:00.456] [LogViewer] [error] [plugin] Failed to connect to provider
[2026-01-02 12:00:00.789] [LogViewer] [warning] [plugin] API key not configured
```

## Configuration

### Plugin Metadata

The plugin provides metadata via JSON:

```json
{
    "id": "ai_provider",
    "name": "AI Provider",
    "version": "1.0.0",
    "apiVersion": "1.0.0",
    "author": "LogViewer Team",
    "description": "AI-powered log analysis with multiple provider support",
    "website": "",
    "type": 4
}
```

### Service Settings

The AI service is configured via JSON settings:

```json
{
    "provider": "ollama",
    "apiKey": "",
    "baseUrl": "http://localhost:11434",
    "model": "qwen2.5-coder:7b"
}
```

**Providers:**
- `ollama` - Local Ollama instance
- `lmstudio` - Local LM Studio instance
- `openai` - OpenAI API
- `anthropic` - Anthropic Claude API
- `google` - Google Gemini API
- `xai` - xAI Grok API

### Storage Locations

**Windows:**
- Plugin: `%APPDATA%\LogViewer\plugins\ai_provider\`
- Config: `%APPDATA%\LogViewer\plugins\ai_provider\config.json`
- Logs: Application logs at `%APPDATA%\LogViewer\log_*.txt`

**macOS:**
- Plugin: `~/Library/Application Support/LogViewer/plugins/ai_provider/`
- Config: `~/Library/Application Support/LogViewer/plugins/ai_provider/config.json`

**Linux:**
- Plugin: `~/.local/share/LogViewer/plugins/ai_provider/`
- Config: `~/.local/share/LogViewer/plugins/ai_provider/config.json`

## Dependencies

The plugin requires:
- **libcurl** - HTTP requests to AI providers
- **Qt6 Widgets** - UI components
- **nlohmann_json** - JSON parsing
- **fmt** - String formatting

All dependencies are bundled in the plugin's `lib/` directory.

## Installation

### From ZIP (User Installation)

1. Download `ai_provider.zip` from releases
2. Open LogViewer → Settings → Plugins
3. Click "Install Plugin" and select the ZIP file
4. Plugin extracts to AppData and loads automatically

### Manual Installation (Development)

1. Build the plugin: `cmake --build build --target ai_provider`
2. Copy plugin files:
   ```bash
   # Windows
   cp build/plugins/ai/ai_provider.dll %APPDATA%\LogViewer\plugins\ai_provider\
   cp -r build/plugins/ai/lib %APPDATA%\LogViewer\plugins\ai_provider\
   cp build/plugins/ai/config.json %APPDATA%\LogViewer\plugins\ai_provider\
   ```
3. Restart LogViewer
4. Enable plugin in Settings → Plugins

## Troubleshooting

### Plugin Not Loading

Check logs for errors:
```
[LogViewer] [error] PluginManager: Failed to load library: ...
[LogViewer] [warning] PluginManager: Plugin_SetLoggerCallback not found
```

**Solutions:**
- Ensure all DLL dependencies are in `lib/` directory
- Check file permissions
- Verify config.json exists

### Configuration Panel Not Visible

Check logs for:
```
[LogViewer] [info] [MainWindow] Plugin ai_provider does not provide a configuration UI
[LogViewer] [debug] PluginManager: Probed panel exports for ai_provider: left=false
```

**Solutions:**
- Ensure plugin DLL has `Plugin_CreateLeftPanel` export
- Rebuild plugin with latest code
- Check plugin version matches application

### Logger Not Working

Check for:
```
[LogViewer] [warning] PluginManager: Plugin_SetLoggerCallback not found - plugin logs may not work
```

**Solution:**
- Ensure plugin exports `Plugin_SetLoggerCallback`
- Rebuild plugin with updated plugin_api headers

### Service Creation Fails

Check for:
```
[plugin] [error] [AIProviderPlugin] Failed to create service: ...
[plugin] [warning] ensureAnalyzer: m_aiService or m_aiService->service is null
```

**Solutions:**
- Verify AI provider is running (e.g., `ollama serve`)
- Check API key configuration
- Test connection from configuration panel
- Review base URL format (must include `http://` or `https://`)

## Development

### Building the Plugin

```bash
# Configure
cmake --preset windows-release-qt

# Build plugin only
cmake --build build/windows-release-qt --target ai_provider

# Build plugin ZIP
cmake --build build/windows-release-qt --target create_ai_provider_zip
```

### Adding Logging

```cpp
#include "PluginLoggerC.h"

void MyFunction() {
    PluginLogger_Log(PLUGIN_LOG_INFO, "Function called");
    
    std::string msg = fmt::format("Processing {} events", count);
    PluginLogger_Log(PLUGIN_LOG_DEBUG, msg.c_str());
}
```

### Creating New Panels

```cpp
// In AIProviderPlugin.hpp
QWidget* CreateCustomPanel(QWidget* parent);

// In AIProviderPlugin.cpp
QWidget* AIProviderPlugin::CreateCustomPanel(QWidget* parent) {
    PluginLogger_Log(PLUGIN_LOG_INFO, "Creating custom panel");
    auto* panel = new MyCustomWidget(parent);
    return panel;
}

// Export via C-ABI
extern "C" {
EXPORT_PLUGIN_SYMBOL void* Plugin_CreateCustomPanel(PluginHandle h, void* parent, const char* settings) {
    auto* plugin = reinterpret_cast<AIProviderPlugin*>(h);
    QWidget* parentWidget = reinterpret_cast<QWidget*>(parent);
    return plugin->CreateCustomPanel(parentWidget);
}
}
```

## Testing

### Manual Testing

1. Enable debug logging in LogViewer
2. Install plugin via ZIP or manual copy
3. Enable plugin in Settings → Plugins
4. Check logs for:
   - `PluginManager: Set logger callback for C-ABI plugin`
   - `[plugin] Plugin_CreateLeftPanel called`
   - `[plugin] GetConfigurationUI called`
   - `[plugin] Created AIConfigPanel`
5. Verify three tabs appear:
   - Left dock: "AI Provider" tab alongside "Filters"
   - Main content: "AI Provider" tab
   - Bottom dock: "AI Provider Chat" tab

### Automated Testing

See `tests/PluginManagerTest.cpp` for plugin loading tests.

## Future Enhancements

- [ ] Plugin settings UI integration with main settings dialog
- [ ] Multiple AI service instances (one per provider)
- [ ] Streaming responses for chat
- [ ] Prompt templates library
- [ ] Analysis history
- [ ] Export analysis results
- [ ] Plugin update notifications
- [ ] Automatic dependency resolution
