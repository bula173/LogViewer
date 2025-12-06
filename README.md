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
cmake --preset macos-debug

# Build
cmake --build --preset macos-debug-build

# Run
./build/macos-debug/src/LogViewer.app/Contents/MacOS/LogViewer
```

### Quick Start (wxWidgets - macOS)

```bash
# Configure with wxWidgets
cmake --preset macos-debug -DGUI_FRAMEWORK=WX

# Build
cmake --build --preset macos-debug-build

# Run
./build/macos-debug/src/LogViewer.app/Contents/MacOS/LogViewer
```

### Platform-Specific Presets

All presets support `-DGUI_FRAMEWORK=WX` or `-DGUI_FRAMEWORK=QT` (default).

**macOS:**
```bash
# Qt (default)
cmake --preset macos-debug           # or macos-release
cmake --build --preset macos-debug-build

# wxWidgets
cmake --preset macos-debug -DGUI_FRAMEWORK=WX
cmake --build --preset macos-debug-build
```

**Windows (MSYS2):**
```bash
# Qt (default)
cmake --preset windows-msys-debug    # or windows-msys-release
cmake --build --preset windows-msys-debug-build

# wxWidgets
cmake --preset windows-msys-debug -DGUI_FRAMEWORK=WX
cmake --build --preset windows-msys-debug-build
```

**Linux:**
```bash
# Qt (default)
cmake --preset linux-debug           # or linux-release
cmake --build --preset linux-debug-build

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
│   ├── application/
│   │   ├── ai/              # AI provider implementations (Qt only)
│   │   ├── config/          # Configuration management
│   │   ├── filters/         # Filtering system
│   │   └── ui/
│   │       ├── qt/          # Qt 6 UI components
│   │       └── wx/          # wxWidgets UI components
│   ├── db/                  # Data model (EventsContainer)
│   ├── parser/              # Log parsers (XML, JSON)
│   └── main/                # Application entry point
├── tests/                   # Unit tests (Google Test)
├── docs/                    # Documentation
├── etc/                     # Default config and examples
└── thirdparty/             # Third-party dependencies
```

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
