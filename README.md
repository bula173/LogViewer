# LogViewer

A modern, cross-platform log viewer with AI-assisted analysis, flexible filtering, and customizable visualization.

**Dual GUI Framework Support:**
- **Qt 6** (default): Modern, dock-based flexible UI
- **wxWidgets 3.2+**: Classic native UI with virtual list controls

## Features

### Core Functionality
- **Multiple Log Format Support**: XML, JSON, and custom log formats
- **High Performance**: Handles millions of log entries with virtual list architecture
- **Advanced Filtering**: Text search, regex, type-based filters with configurable field selection
- **Customizable Display**: Configurable columns, colors, and movable/resizable panels
- **Flexible UI**: 
  - **Qt**: Dock-based layout - move, float, or hide panels as needed
  - **wxWidgets**: Traditional split-pane layout with virtual list controls

### AI-Assisted Analysis (Qt version)
- **Multiple AI Providers**: Ollama (local), LM Studio (local), OpenAI, Anthropic Claude, Google Gemini, xAI Grok
- **Smart Log Analysis**: Automatic pattern detection, error summarization, and insights
- **Filter-Aware**: AI respects your current filters and type field configuration
- **Custom Prompts**: Save and load analysis prompts for different log types
- **Configurable Timeout**: Adjust timeout (30-3600s) for large analyses or slow machines

### Configuration
- **Configurable Type Filter Field**: Use any log field for filtering (`type`, `level`, `severity`, `priority`, etc.)
- **Custom Color Schemes**: Define colors for any log entry type or level
- **Persistent Settings**: JSON-based configuration with platform-specific storage
- **Export/Import**: Save and share your analysis prompts

## Architecture

**Dual UI Framework:**
- **Qt 6**: QMainWindow, QDockWidget, QTableView with Model/View architecture
- **wxWidgets**: wxFrame, wxDataViewCtrl with virtual list controls

**Common Core:**
- **Model**: `db::EventsContainer` - unified data model for both frameworks
- **Parsing**: Modular parser system (XML, JSON support)
- **AI Integration** (Qt only): Factory pattern with multiple provider implementations
- **Threading**: 
  - Qt: QtConcurrent for non-blocking AI requests
  - wxWidgets: wxThread for background parsing
- **Error Handling**: Structured error types with user-friendly messages

## Requirements

### Essential (Both Frameworks)
- **CMake** 3.25 or higher
- **C++20 Compiler**: 
  - Clang 15+
  - GCC 11+
  - MSVC 2019+

### GUI Framework (Choose One)

**For Qt Build (Default):**
- **Qt 6**: 6.5 or higher
  - macOS: `brew install qt@6`
  - Linux: `sudo apt-get install qt6-base-dev`
  - Windows: Download from qt.io or use vcpkg
- **CURL**: For AI provider HTTP requests (usually pre-installed on macOS/Linux)

**For wxWidgets Build:**
- **wxWidgets**: 3.2 or higher
  - macOS: `brew install wxwidgets`
  - Linux: `sudo apt-get install libwxgtk3.2-dev`
  - Windows: Download from wxwidgets.org or use vcpkg

### Optional
- **Ninja**: Faster builds
- **Doxygen**: Documentation generation
- **clang-format**: Code formatting
- **cppcheck**: Static analysis

## Building

### Choosing GUI Framework

The project supports two GUI frameworks. Qt is the default and recommended for new features (AI integration). wxWidgets provides a traditional native UI experience.

**Build with Qt (Default):**
```bash
cmake --preset macos-debug              # GUI_FRAMEWORK=QT (default)
```

**Build with wxWidgets:**
```bash
cmake --preset macos-debug -DGUI_FRAMEWORK=WX
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

### Quick Start (wxWidgets - macOS)

```bash
# Configure with wxWidgets
cmake --preset macos-debug-wx

# Build
cmake --build --preset macos-debug-build-wx

# Run
./build/macos-debug-wx/bin/LogViewerWx.app/Contents/MacOS/LogViewerWx
```

### Platform-Specific Presets

All presets support `-DGUI_FRAMEWORK=WX` or `-DGUI_FRAMEWORK=QT` (default).

**macOS:**
```bash
# Qt (default)
cmake --preset macos-debug-qt           # or macos-release-qt
cmake --build --preset macos-debug-build-qt

# wxWidgets
cmake --preset macos-debug-wx           # or macos-release-wx
cmake --build --preset macos-debug-build-wx
```

**Windows (MSYS2):**
```bash
# Qt (default)
cmake --preset windows-msys-debug-qt    # or windows-msys-release-qt
cmake --build --preset windows-msys-debug-build-qt

# wxWidgets
cmake --preset windows-msys-debug-wx    # or windows-msys-release-wx
cmake --build --preset windows-msys-debug-build-wx
```

**Linux:**
```bash
# Qt (default)
cmake --preset linux-debug-qt           # or linux-release-qt
cmake --build --preset linux-debug-build-qt

# wxWidgets
cmake --preset linux-debug -DGUI_FRAMEWORK=WX
cmake --build --preset linux-debug-build
```

### Using VS Code Tasks

The project includes pre-configured tasks (Cmd/Ctrl+Shift+P > "Tasks: Run Task"):

**Note:** VS Code tasks currently configured for Qt builds. For wxWidgets, use command line with `-DGUI_FRAMEWORK=WX`.

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

## Configuration

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
  },
  "aiConfig": {
    "provider": "ollama",
    "apiKey": "",
    "ollamaBaseUrl": "http://localhost:11434",
    "ollamaDefaultModel": "llama3.2:latest",
    "timeoutSeconds": 300
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

### Basic Workflow (Both Qt and wxWidgets)

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
│   │       └── wx/          # wxWidgets UI components
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

## Framework Comparison

| Feature | Qt 6 | wxWidgets |
|---------|------|-----------|
| **UI Style** | Modern dock-based | Traditional split-pane |
| **AI Integration** | ✅ Full support | ❌ Not available |
| **Movable Panels** | ✅ Yes | ❌ Fixed layout |
| **Column Reordering** | ✅ Yes | ⚠️ Limited |
| **Performance** | Excellent | Excellent |
| **Native Look** | Good | Excellent |
| **Platform Support** | Windows, macOS, Linux | Windows, macOS, Linux |
| **License** | LGPL v3 (dynamically linked) | wxWindows (LGPL-like) |

**Recommendation:** Use Qt for new deployments (AI features, modern UI). Use wxWidgets for traditional native UI or specific platform requirements.

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
- **wxWidgets**: wxWindows License (LGPL-like) - wxWidgets version only
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
# Explicitly choose framework
cmake --preset macos-debug -DGUI_FRAMEWORK=QT   # or WX
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

**wxWidgets not found:**
```bash
# macOS
brew install wxwidgets

# Linux  
sudo apt-get install libwxgtk3.2-dev

# Windows (MSYS2)
pacman -S mingw-w64-x86_64-wxwidgets3.2-msw
```

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

- Original wxWidgets template and foundation by lszl84
- Qt Framework by The Qt Company
- wxWidgets by wxWidgets team
- AI providers: Ollama, OpenAI, Anthropic, Google, xAI
- Third-party libraries: nlohmann/json, spdlog, CURL, Google Test
