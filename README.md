# LogViewer

A modern, cross-platform log viewer with AI-assisted analysis, flexible filtering, and customizable visualization.

**GUI Framework:**
- **Qt 6 (required)**: Modern, dock-based flexible UI used for all current features including AI integration

## Recent Updates (January 2026 - v1.0.0)

### Plugin System Improvements
- **Fixed Plugin Logging**: Implemented callback-based logging mechanism that works across DLL boundaries
  - Application calls `Plugin_SetLoggerCallback` after plugin creation
  - Plugin messages now appear in application logs with `[plugin]` prefix
  - Resolved ODR violation with inline static variables
  
- **Configuration Panel Integration**: Fixed plugin configuration UI display
  - Added `Plugin_CreateLeftPanel` C-ABI export for configuration widgets
  - Configuration panel now appears as separate tab in left dock alongside "Filters"
  - Previously panels were incorrectly embedded inside the Filters tab
  - Proper parent widget management ensures correct UI hierarchy

- **Enhanced Diagnostics**: Added comprehensive logging throughout plugin lifecycle
  - Plugin creation, service initialization, and panel creation are fully logged
  - Error messages clearly indicate failure points
  - Easier debugging of plugin integration issues

- **Event Management & Sorting**: Improved event merge strategy and sorting behavior
  - Sequential ID reassignment with original_id preservation for all events
  - ID-based tie-breaking when sorting by timestamp or other columns
  - Source and original_id columns auto-hide when no data available

- **User Experience**: Recent files list with persistent storage
  - Quick access to frequently opened files from File menu
  - Automatic cleanup of files that no longer exist

For detailed plugin documentation, see:
- `docs/PLUGIN_SYSTEM.md` - Plugin architecture overview
- `docs/AI_PROVIDER_PLUGIN.md` - AI Provider plugin specifics
- `docs/PLUGIN_IMPLEMENTATION.md` - Implementation details

## Features

### Core Functionality
- **Multiple Log Format Support**: XML, JSON, and custom log formats
- **High Performance**: Handles millions of log entries with virtual list architecture
- **Advanced Filtering**: Text search, regex, type-based filters with configurable field selection
- **Customizable Display**: Configurable columns, colors, and movable/resizable panels
- **Flexible UI**: 
 - **Flexible UI (Qt)**: Dock-based layout - move, float, or hide panels as needed

### AI-Assisted Analysis (Qt version)
- **Multiple AI Providers**: Ollama (local), LM Studio (local), OpenAI, Anthropic Claude, Google Gemini, xAI Grok
- **Plugin Architecture**: AI providers implemented as C-ABI plugins for better compatibility
- **Smart Log Analysis**: Automatic pattern detection, error summarization, and insights
- **Filter-Aware**: AI respects your current filters and type field configuration
- **Custom Prompts**: Save and load analysis prompts for different log types
- **Configurable Timeout**: Adjust timeout (30-3600s) for large analyses or slow machines
- **Plugin Configuration UI**: Each plugin can provide its own configuration panel in the left dock

### Plugin System
- **C-ABI Architecture**: Plugins use C function exports for better compatibility across compiler versions
- **Dynamic Loading**: Plugins loaded from ZIP archives or directories
- **Integrated Logging**: Plugins log to application via callback mechanism
- **UI Extension**: Plugins can add panels to main, bottom, left, and right dock areas
- **Configuration Panels**: Plugins provide configuration UI as separate tabs in left panel

### Configuration
- **Configurable Type Filter Field**: Use any log field for filtering (`type`, `level`, `severity`, `priority`, etc.)
- **Custom Color Schemes**: Define colors for any log entry type or level
- **Persistent Settings**: JSON-based configuration with platform-specific storage
- **Export/Import**: Save and share your analysis prompts

## Architecture

## Architecture

**UI Framework:**
- **Qt 6**: QMainWindow, QDockWidget, QTableView with Model/View architecture (current and supported UI)

**Common Core:**
- **Model**: `db::EventsContainer` - unified data model
- **Parsing**: Modular parser system (XML, JSON support)
- **AI Integration**: Implemented for the Qt UI using a provider factory
- **Threading**: QtConcurrent used for non-blocking operations
- **Error Handling**: Structured error types with user-friendly messages

## Requirements

### Essential (Both Frameworks)
- **CMake** 3.25 or higher
- **C++20 Compiler**: 
  - Clang 15+
  - GCC 11+
  - MSVC 2019+

### GUI Framework and Requirements

The project now targets **Qt 6** exclusively. Qt 6 (6.5+) is required to build the full feature set including AI providers and the plugin UI integration.

- macOS: `brew install qt@6`
- Linux: `sudo apt-get install qt6-base-dev`
- Windows: Download from qt.io or use vcpkg

**CURL**: For AI provider HTTP requests (usually pre-installed on macOS/Linux)

### Optional
- **Ninja**: Faster builds
- **Doxygen**: Documentation generation
- **clang-format**: Code formatting
- **cppcheck**: Static analysis

## Building

### Choosing GUI Framework

The project targets Qt 6. Qt is the supported and recommended framework for new features (AI integration).

**Build with Qt (Default):**
```bash
cmake --preset macos-debug-qt
```

### Quick Start (Qt - macOS)

```bash
# Configure
cmake --preset macos-debug-qt

# Build
cmake --build --preset macos-debug-build-qt

# Run
./build/macos-debug-qt/bin/LogViewer.app/Contents/MacOS/LogViewer

# Or use the script
./scripts/run_debug.sh
```

### Platform-Specific Presets

Presets are configured for Qt builds across macOS, Windows, and Linux. Use the provided presets for quick configuration and building.

Examples:
```bash
# macOS (Qt)
cmake --preset macos-debug-qt
cmake --build --preset macos-debug-build-qt

# Windows (MSYS2, Qt)
cmake --preset windows-msys-debug-qt
cmake --build --preset windows-msys-debug-build-qt

# Linux (Qt)
cmake --preset linux-debug-qt
cmake --build --preset linux-debug-build-qt
```

### Using VS Code Tasks

The project includes pre-configured tasks (Cmd/Ctrl+Shift+P > "Tasks: Run Task"):

**Note:** VS Code tasks are configured for Qt builds.

- **Build Debug** - Configure and build debug version (Qt)
- **Build Release** - Configure and build release version (Qt)
- **Run App (Debug)** - Build and launch application
- **Test Debug** - Run unit tests
- **Static Analysis** - Run cppcheck
- **Package Release** - Create distributable package

### Running Tests

```bash
ctest --preset macos-debug-test      # or windows-msys-debug-test, linux-debug-test
```

## Plugin Development

LogViewer has an official **Plugin SDK** for extending functionality with custom plugins.

### Getting Started with Plugins

Quick references for plugin developers:

1. **[SDK Getting Started Guide](docs/SDK_GETTING_STARTED.md)** — Complete walkthrough with examples
2. **[SDK Quick Reference](docs/SDK_QUICK_REFERENCE.md)** — Syntax reference and common patterns  
3. **[Basic Plugin Example](examples/BasicPlugin/)** — Minimal working plugin to study

### Building the Example Plugin

```bash
# Build and install LogViewer SDK
cmake -S . -B build -DBUILD_SDK=ON
cmake --install build --prefix ~/LogViewer_install

# Build the BasicPlugin example
cd examples/BasicPlugin
cmake -S . -B build -DCMAKE_PREFIX_PATH=~/LogViewer_install/sdk/lib/cmake
cmake --build build
```

### Plugin Features

- **C-ABI Interface**: Stable binary interface across compilers
- **Automatic Dependency Discovery**: `find_package(LogViewer CONFIG REQUIRED)` provides Qt6, Threads, JSON, formatting libraries, and more
- **Logger Callback**: Plugins log to application via callback
- **UI Panels**: Create Qt widgets for left, right, bottom, or main areas
- **Event Access**: Read log events via event container API
- **Cross-Platform**: Build once, run on Windows, macOS, Linux

### Plugin Documentation

For detailed plugin development:
- [Plugin System Architecture](docs/PLUGIN_SYSTEM.md)
- [Plugin Implementation Guide](docs/PLUGIN_IMPLEMENTATION.md)
- [AI Provider Plugin](docs/CLOUD_AI_INTEGRATION.md)


**Note:** AI features and advanced configuration UI are only available in the Qt version.

Configuration is stored in platform-specific locations:
- **macOS**: `~/Library/Application Support/LogViewer/config.json`
- **Linux**: `~/.config/LogViewer/config.json`
- **Windows**: `%APPDATA%/LogViewer/config.json`

### Key Configuration Options

```json
{
  "filters": {
    "typeFilterField": "level"
  },
  "columns": ["timestamp", "level", "message", "source"],
  "columnColors": {
    "level": {
      "ERROR": {"foreground": "#ffffff", "background": "#dc3545"},
      "WARN": {"foreground": "#000000", "background": "#ffc107"},
      "INFO": {"foreground": "#000000", "background": "#28a745"},
      "DEBUG": {"foreground": "#ffffff", "background": "#6c757d"}
    }
  }
}
```

## AI Setup

**Note:** AI features are only available in the Qt version of the application.

### Local AI (Recommended for Privacy)

**Ollama:**
```bash
# Install Ollama
brew install ollama  # macOS
# or download from https://ollama.ai

# Start Ollama
ollama serve

# Pull a model
ollama pull llama3.2
```

**LM Studio:**
1. Download from https://lmstudio.ai
2. Load a model
3. Start local server
4. Configure LogViewer to use `http://localhost:1234`

### Cloud AI Providers

Configure in Settings > AI:
- **OpenAI**: Requires API key from https://platform.openai.com
- **Anthropic**: Requires API key from https://console.anthropic.com
- **Google Gemini**: Requires API key from https://ai.google.dev
- **xAI**: Requires API key from https://x.ai

## Usage

### Basic Workflow (Qt)

1. **Open Log File**: File > Open (or Cmd/Ctrl+O)
2. **Apply Filters**: Use the Filters panel to filter by type, text, or regex
3. **View Details**: Select an entry to see full details in the Details panel
4. **Customize**: Configure columns and colors in Settings

### AI Analysis (Qt Only)

5. **AI Analysis**: 
   - Select events or apply filters
   - Open Tools > AI Analysis (bottom panel)
   - Write or load a prompt
   - Click Analyze
6. **Advanced UI**:
   - Move panels by dragging title bars
   - Float panels by double-clicking title bars

## Documentation

- **Architecture**: `docs/ARCHITECTURE.md` - System design and patterns
- **Development Guide**: `docs/DEVELOPMENT.md` - Build system, code style, AI integration
- **AI Analysis Guide**: `docs/AI_ANALYSIS_GUIDE.md` - Using AI features effectively
- **UI Improvements**: `docs/UI_IMPROVEMENTS.md` - Dock widgets and configuration
- **Build System**: `docs/CMAKE_PRESETS_INTEGRATION.md` - CMake presets documentation

## Project Structure

```
LogViewer/
├── src/
│   ├── application/         # Core application logic
│   │   ├── ai/              # AI provider implementations (Qt only)
│   │   ├── config/          # Configuration management
│   │   ├── filters/         # Filtering system
│   │   ├── plugins/         # Plugin system interfaces
│   │   └── ui/
│   │       ├── qt/          # Qt 6 UI components
│   │       └──              # wxWidgets UI components removed
│   ├── db/                  # Data model (EventsContainer)
│   ├── parser/              # Log parsers (XML, JSON)
│   ├── plugins/             # Plugin implementations
│   │   ├── ai/              # AI provider plugin
│   │   ├── event_metrics/   # Event metrics plugin
│   │   └── ssh_parser/      # SSH log parser plugin
│   └── main/                # Application entry point
├── build/                   # Build outputs (not in git)
│   └── <preset-name>/
│       ├── bin/             # Executables (development)
│       ├── lib/             # Libraries
│       └── plugins/         # Plugin shared libraries
├── dist/                    # Distribution files
│   ├── packages/            # Final installers (.dmg, .zip, .exe)
│   └── staging/             # Install staging area (not in git)
├── tests/                   # Unit tests (Google Test)
├── docs/                    # Documentation
├── etc/                     # Default config and examples
└── thirdparty/              # Third-party dependencies
```

## Build Output Structure

**Development (build directory):**
- Build outputs go to `build/<preset-name>/bin/` and `build/<preset-name>/lib/`
- Run directly from build directory for development
- Plugins built to `build/<preset-name>/plugins/`

**Distribution (dist directory):**
- Run `cmake --install` to create deployable application in `dist/staging/`
- Run `cpack` to create installers in `dist/packages/`
- Only `dist/packages/` contents are suitable for distribution

## Framework

The project uses `Qt 6` as the supported UI framework. AI integration and plugin UI extensions are implemented for Qt.

## Installation and Packaging

### Creating Distributable Packages

After building, you can create distributable packages:

**macOS (.dmg):**
```bash
# Build release
cmake --preset macos-release-qt
cmake --build --preset macos-release-build-qt

# Install to staging
cmake --install build/macos-release-qt --prefix dist/staging

# Create DMG package
cd build/macos-release-qt
cpack --config CPackConfig.cmake

# Package will be in dist/packages/
```

**Windows (.zip with installer):**
```bash
# Build release
cmake --preset windows-msys-release-qt
cmake --build --preset windows-msys-release-build-qt

# Install to staging
cmake --install build/windows-msys-release-qt --prefix dist/staging

# Create package
cd build/windows-msys-release-qt
cpack --config CPackConfig.cmake

# Package will be in dist/packages/
```

**Linux (.tar.gz, .deb):**
```bash
# Build release
cmake --preset linux-release-qt
cmake --build --preset linux-release-build-qt

# Install to staging
cmake --install build/linux-release-qt --prefix dist/staging

# Create packages
cd build/linux-release-qt
cpack --config CPackConfig.cmake

# Packages will be in dist/packages/
```

### Directory Structure After Build

```
build/macos-release-qt/     # Build artifacts
  ├── bin/                  # Development executables
  │   └── LogViewer.app/
  ├── lib/                  # Static libraries
  └── plugins/              # Plugin .dylib files & .zip packages

dist/
  ├── staging/              # Installed application (after cmake --install)
  │   ├── LogViewer.app/
  │   ├── etc/
  │   └── plugins/
  └── packages/             # Final distributable packages
      └── LogViewer-1.0.0-Darwin.dmg
```

## Installation and Packaging

### Creating Distributable Packages

After building, you can create distributable packages:

**macOS (.dmg):**
```bash
# Build release
cmake --preset macos-release-qt
cmake --build --preset macos-release-build-qt

# Install to staging
cmake --install build/macos-release-qt --prefix dist/staging

# Create DMG package
cd build/macos-release-qt
cpack --config CPackConfig.cmake

# Package will be in dist/packages/
```

**Windows (.zip with installer):**
```bash
# Build release
cmake --preset windows-msys-release-qt
cmake --build --preset windows-msys-release-build-qt

# Install to staging
cmake --install build/windows-msys-release-qt --prefix dist/staging

# Create package
cd build/windows-msys-release-qt
cpack --config CPackConfig.cmake

# Package will be in dist/packages/
```

**Linux (.tar.gz, .deb):**
```bash
# Build release
cmake --preset linux-release-qt
cmake --build --preset linux-release-build-qt

# Install to staging
cmake --install build/linux-release-qt --prefix dist/staging

# Create packages
cd build/linux-release-qt
cpack --config CPackConfig.cmake

# Packages will be in dist/packages/
```

### Directory Structure After Build

```
build/macos-release-qt/     # Build artifacts
  ├── bin/                  # Development executables
  │   └── LogViewer.app/
  ├── lib/                  # Static libraries
  └── plugins/              # Plugin .dylib files & .zip packages

dist/
  ├── staging/              # Installed application (after cmake --install)
  │   ├── LogViewer.app/
  │   ├── etc/
  │   └── plugins/
  └── packages/             # Final distributable packages
      └── LogViewer-1.0.0-Darwin.dmg
```

## License

This project is licensed under the MIT License - see `license.md` for details.

**Third-Party Licenses:**
- **Qt 6**: LGPL v3 (dynamically linked) - Qt version only
**Third-Party Licenses:**
- **Qt 6**: LGPL v3 (dynamically linked)
- **Other libraries**: MIT, BSD (see `license.md` for complete list)

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/amazing-feature`)
3. Follow code style guidelines in `docs/DEVELOPMENT.md`
4. Add tests for new functionality
5. Ensure all tests pass (`ctest --preset <platform>-debug-test`)
6. Commit changes (`git commit -m 'Add amazing feature'`)
7. Push to branch (`git push origin feature/amazing-feature`)
8. Open a Pull Request

## Troubleshooting

### Build Issues

**GUI Framework selection:**
```bash
# Explicitly choose framework (Qt is supported)
cmake --preset macos-debug -DGUI_FRAMEWORK=QT
```

**Qt not found:**
```bash
# macOS
brew install qt@6
export CMAKE_PREFIX_PATH="$(brew --prefix qt@6)"

# Linux
sudo apt-get install qt6-base-dev

# Windows
# Install Qt from qt.io or use vcpkg
```

(wxWidgets support removed from this README)

**CMake version too old:**
```bash
# macOS
brew upgrade cmake

# Linux
# Download latest from https://cmake.org/download/
```

### Runtime Issues

**AI features not available:**
- AI integration is only available in the Qt version
- Rebuild with `-DGUI_FRAMEWORK=QT` (default)

**AI timeout errors:**
- Increase timeout in Settings > AI > Timeout (up to 3600 seconds)
- Use local AI providers (Ollama/LM Studio) for better performance
- Reduce number of events being analyzed

**AI plugin configuration panel not visible:**
- Ensure plugin is installed and enabled in Settings > Plugins
- Check application logs for `[plugin]` messages
- Configuration panel appears as separate tab in left dock alongside "Filters"
- Restart application if plugin was just installed

**Plugin logs not appearing:**
- Plugins use callback-based logging via `Plugin_SetLoggerCallback`
- Check logs for: `PluginManager: Set logger callback for C-ABI plugin`
- If missing, rebuild plugin with latest `plugin_api` headers
- Plugin messages appear with `[plugin]` prefix in logs

**File dialog not working (macOS):**
- Known issue with native dialogs - already using non-native dialogs

**Configuration not persisting:**
- Check file permissions in config directory
- See platform-specific paths above

### Packaging Issues

**DMG creation fails on macOS (CI/GitHub Actions):**
- The custom DS_Store AppleScript is automatically disabled in CI environments
- Local builds use custom icon layout; CI builds use default layout
- Both produce functional DMG files

**Package task not found:**
```bash
# Build release first, then package
cmake --preset macos-release
cmake --build --preset macos-release-build
cmake --build --preset macos-release-build --target package
```

## Support

- **Issues**: https://github.com/bula173/LogViewer/issues
- **Discussions**: https://github.com/bula173/LogViewer/discussions
- **Documentation**: `docs/` directory

## Acknowledgments
- Qt Framework by The Qt Company
- Project templates and earlier foundation contributions
- AI providers: Ollama, OpenAI, Anthropic, Google, xAI
- Third-party libraries: nlohmann/json, spdlog, CURL, Google Test
 
