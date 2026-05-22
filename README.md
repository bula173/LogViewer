# LogViewer

A modern, cross-platform log viewer built with Qt 6 and C++20, featuring AI-assisted analysis, flexible filtering, and customizable visualization.

## Features

### Core
- **Multiple Log Formats**: XML, CSV, and CAN/ASC (Vector CANalyzer) — JSON coming soon
- **DBC Signal Decoding**: Load a `.dbc` file alongside an ASC log to decode raw CAN frames into named signals
- **High Performance**: Virtual list architecture handles millions of log entries
- **Advanced Filtering**: Text search, regex, time-range, and type-based filters with configurable field selection
- **Filter Profiles**: Save and restore named filter states for quick context switching
- **Flexible UI**: Dock-based layout — move, float, or hide panels as needed
- **Persistent Settings**: JSON-based configuration with platform-specific storage

### Analysis Panels
- **Signal Plot**: Plot decoded CAN signal values (`SIG:*` fields) over time — select any combination with checkboxes; auto-downsamples to 2 000 points per series
- **Statistics**: Format-aware summary — generic key metrics for all formats; CAN-specific frame breakdown (Rx/Tx/error counts, unique IDs, frame rate) and signal ranges (min/max/avg) for ASC files
- **Timeline Chart**: Interactive bar histogram of event volume over time with zoom and brush selection
- **Pattern Analysis**: Template clustering that groups structurally similar log lines
- **Trace Viewer**: Sequence-diagram-style view grouped by actor
- **Bookmarks**: Annotate and navigate to important events
- **Scenarios**: Define multi-step event sequences and export matches as JSON Lines
- **Actors**: Event attribution tree built from configurable actor definitions

### AI-Assisted Analysis
- **Multiple Providers**: Ollama (local), LM Studio (local), OpenAI, Anthropic Claude, Google Gemini, xAI Grok
- **Smart Analysis**: Automatic pattern detection, error summarization, and insights
- **Filter-Aware**: AI respects your current filters and type field configuration
- **Custom Prompts**: Save and load analysis prompts for different log types

### Plugin System
- **C-ABI Interface**: Stable binary interface across compiler versions
- **Dynamic Loading**: Plugins loaded from ZIP archives or directories
- **UI Extension**: Plugins can add panels to main, bottom, left, and right dock areas
- **Configuration Panels**: Each plugin provides its own configuration tab in the left dock
- **Integrated Logging**: Plugins log to the application via callback mechanism

## Requirements

| Component | Version |
|-----------|---------|
| CMake | 3.25+ |
| C++ Compiler | Clang 15+, GCC 11+, or MSVC 2019+ |
| Qt | 6.5+ |
| Ninja | Recommended |

**Install Qt:**
```bash
# macOS
brew install qt@6

# Linux (Debian/Ubuntu)
sudo apt-get install qt6-base-dev qt6-tools-dev

# Windows — download from qt.io or use vcpkg
```

## Building

### Quick Start

```bash
# macOS
cmake --preset macos-debug-qt
cmake --build --preset macos-debug-build-qt
./build/macos-debug-qt/src/main/LogViewer.app/Contents/MacOS/LogViewer

# Linux
cmake --preset linux-debug-qt
cmake --build --preset linux-debug-build-qt
./build/linux-debug-qt/bin/LogViewer

# Windows (MSYS2)
cmake --preset windows-msys-debug-qt
cmake --build --preset windows-msys-debug-qt-build
```

### Release Builds

```bash
# macOS
cmake --preset macos-release-qt
cmake --build --preset macos-release-build-qt

# Linux
cmake --preset linux-release-qt
cmake --build --preset linux-release-build-qt

# Windows
cmake --preset windows-msys-release-qt
cmake --build --preset windows-msys-release-qt-build
```

### Running Tests

```bash
ctest --preset macos-debug-test-qt      # macOS
ctest --preset linux-debug-test-qt      # Linux
ctest --preset windows-msys-debug-qt-test  # Windows
```

## Code Quality Tools

The build system includes targets for static analysis, sanitizers, and formatting.

### Sanitizers (debug presets)

The `macos-debug-qt` and `linux-debug-qt` presets enable AddressSanitizer + UBSan automatically. Controlled via:

```bash
cmake --preset macos-debug-qt -DLOGVIEWER_SANITIZER=Address      # ASan + UBSan (default)
cmake --preset macos-debug-qt -DLOGVIEWER_SANITIZER=Thread        # TSan
cmake --preset macos-debug-qt -DLOGVIEWER_SANITIZER=UndefinedBehavior  # UBSan only
cmake --preset macos-debug-qt -DLOGVIEWER_SANITIZER=None          # disabled
```

### Static Analysis

```bash
# cppcheck (requires ENABLE_CPPCHECK=ON, set in debug presets)
cmake --build --preset macos-cppcheck
cmake --build --preset linux-cppcheck

# clang-tidy
cmake --build --preset macos-clang-tidy
cmake --build --preset linux-clang-tidy

# Run all quality tools (cppcheck + tidy + format check)
cmake --build --preset macos-quality-tools
cmake --build --preset linux-quality-tools
```

### Formatting

```bash
# Check formatting (CI mode — exits non-zero on violations)
cmake --build build/macos-debug-qt --target format-check

# Apply formatting in-place
cmake --build --preset macos-format
cmake --build --preset linux-format
```

See `docs/CPPCHECK.md` and `docs/SANITIZERS.md` for details.

## Plugin Development

LogViewer provides an official **Plugin SDK** for extending functionality.

### Getting Started

1. **[SDK Getting Started Guide](docs/SDK_GETTING_STARTED.md)** — Complete walkthrough with examples
2. **[SDK Quick Reference](docs/SDK_QUICK_REFERENCE.md)** — Syntax reference and common patterns
3. **[Basic Plugin Example](examples/BasicPlugin/)** — Minimal working plugin

### Building the Example Plugin

```bash
# Build and install the SDK
cmake --preset macos-release-qt
cmake --build --preset macos-release-build-qt
cmake --install build/macos-release-qt --prefix ~/LogViewer_install

# Build the example plugin
cd examples/BasicPlugin
cmake -S . -B build -DCMAKE_PREFIX_PATH=~/LogViewer_install/sdk/lib/cmake
cmake --build build
```

### Plugin Documentation

- [Plugin System Architecture](docs/PLUGIN_SYSTEM.md)
- [Plugin Implementation Guide](docs/PLUGIN_IMPLEMENTATION.md)
- [AI Provider Plugin](docs/AI_PROVIDER_PLUGIN.md)

## Configuration

Settings are stored per-platform:

| Platform | Location |
|----------|----------|
| macOS | `~/Library/Application Support/LogViewer/config.json` |
| Linux | `~/.config/LogViewer/config.json` |
| Windows | `%APPDATA%/LogViewer/config.json` |

Example configuration:
```json
{
  "filters": {
    "typeFilterField": "level"
  },
  "columns": ["timestamp", "level", "message", "source"],
  "columnColors": {
    "level": {
      "ERROR": {"foreground": "#ffffff", "background": "#dc3545"},
      "WARN":  {"foreground": "#000000", "background": "#ffc107"},
      "INFO":  {"foreground": "#000000", "background": "#28a745"},
      "DEBUG": {"foreground": "#ffffff", "background": "#6c757d"}
    }
  }
}
```

## AI Setup

### Local AI (Recommended for Privacy)

**Ollama:**
```bash
brew install ollama   # macOS
ollama serve
ollama pull llama3.2
```

**LM Studio:** Download from https://lmstudio.ai, load a model, start the local server, and point LogViewer at `http://localhost:1234`.

### Cloud Providers

Configure in Settings > AI with an API key:

| Provider | Key source |
|----------|-----------|
| OpenAI | https://platform.openai.com |
| Anthropic | https://console.anthropic.com |
| Google Gemini | https://ai.google.dev |
| xAI Grok | https://x.ai |

## Usage

1. **Open Log File** — File > Open (Cmd/Ctrl+O); supports XML, CSV, and `.asc` CAN logs
2. **Load DBC** — File > Load DBC… to decode CAN signals from a `.dbc` database (optional, ASC only)
3. **Filter** — Use the Filters panel; filter by type, text, regex, or time range
4. **Inspect** — Select an entry to see full details in the Details panel
5. **Plot Signals** — Switch to the *Signals* tab in the content area; check `SIG:*` fields to plot their values over time (available after loading an ASC + DBC file)
6. **Statistics** — The *Statistics* tab shows format-aware metrics; CAN files additionally show frame counts, Rx/Tx breakdown, unique IDs, frame rate, and signal ranges
7. **AI Analysis** — Open the bottom panel, write or load a prompt, click Analyze
8. **Customize** — Drag panel title bars to rearrange; configure columns and colors in Settings

## Project Structure

```
LogViewer/
├── src/
│   ├── application/         # Core application logic
│   │   ├── config/          # Configuration management
│   │   ├── db/              # EventsContainer and LogEvent data model
│   │   ├── filters/         # Filtering system
│   │   ├── mvc/             # Model-View-Controller patterns
│   │   ├── parsers/         # Log parsers
│   │   │   ├── xml/         # XML parser
│   │   │   ├── csv/         # CSV parser
│   │   │   ├── asc/         # CAN/ASC parser (Vector CANalyzer format)
│   │   │   └── dbc/         # DBC parser + CAN signal decoder
│   │   ├── plugins/         # Plugin system interfaces
│   │   ├── ui/qt/           # Qt 6 UI components
│   │   │   ├── panels/      # Dock and content panels
│   │   │   ├── dialogs/     # Modal dialogs
│   │   │   ├── events/      # Events table model and view
│   │   │   └── utils/       # Shared UI utilities
│   │   ├── util/            # General utilities
│   │   └── version/         # Version info
│   ├── plugin_api/          # C-ABI plugin interface headers
│   └── main/                # Application entry point
├── plugins/                 # First-party plugins
│   └── ai/                  # AI provider plugin
├── examples/BasicPlugin/    # Minimal plugin example
├── tests/                   # Unit tests (Google Test)
├── cmake/                   # Build helper modules
│   ├── Warnings.cmake       # Compiler warning flags
│   ├── EnableSanitizers.cmake
│   ├── ClangTidy.cmake
│   ├── Cppcheck.cmake
│   └── ClangFormat.cmake
├── docs/                    # Documentation
├── etc/                     # Default config files
└── thirdparty/              # Third-party dependencies
```

## Packaging

```bash
# macOS — produces dist/packages/LogViewer-1.0.0-Darwin.dmg
cmake --preset macos-release-qt
cmake --build --preset macos-release-build-qt
cmake --install build/macos-release-qt --prefix dist/staging
cd build/macos-release-qt && cpack --config CPackConfig.cmake

# Linux — produces .tar.gz and .deb
cmake --preset linux-release-qt
cmake --build --preset linux-release-build-qt
cmake --install build/linux-release-qt --prefix dist/staging
cd build/linux-release-qt && cpack --config CPackConfig.cmake

# Windows — produces .zip
cmake --preset windows-msys-release-qt
cmake --build --preset windows-msys-release-qt-build
cmake --install build/windows-msys-release-qt --prefix dist/staging
cd build/windows-msys-release-qt && cpack --config CPackConfig.cmake
```

## Troubleshooting

**Qt not found:**
```bash
# macOS — pass the prefix explicitly
export CMAKE_PREFIX_PATH="$(brew --prefix qt@6)"
cmake --preset macos-debug-qt
```

**CMake version too old:**
```bash
brew upgrade cmake          # macOS
# or download from https://cmake.org/download/
```

**AI timeout errors:** Increase timeout in Settings > AI > Timeout. Use local providers (Ollama/LM Studio) for better performance.

**Plugin config panel not visible:** Ensure the plugin is enabled in Settings > Plugins. The panel appears as a separate tab in the left dock alongside "Filters". Check application logs for `[plugin]` messages.

**Qt initialization crash (`getcwd` failed):** Occurs when the working directory is deleted or unmounted after launch. The application falls back gracefully to `/etc` for config discovery since v1.0.0.

## Documentation

| Document | Description |
|----------|-------------|
| [docs/ARCHITECTURE.md](docs/ARCHITECTURE.md) | System design and patterns |
| [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) | Build system, code style |
| [docs/BUILD_GUIDE.md](docs/BUILD_GUIDE.md) | Detailed build instructions |
| [docs/SDK_GETTING_STARTED.md](docs/SDK_GETTING_STARTED.md) | Plugin SDK quickstart |
| [docs/PLUGIN_SYSTEM.md](docs/PLUGIN_SYSTEM.md) | Plugin architecture |
| [docs/AI_ANALYSIS_GUIDE.md](docs/AI_ANALYSIS_GUIDE.md) | Using AI features |
| [docs/SANITIZERS.md](docs/SANITIZERS.md) | Sanitizer configuration |
| [docs/CPPCHECK.md](docs/CPPCHECK.md) | Static analysis setup |

## Contributing

1. Fork the repository
2. Create a feature branch (`git checkout -b feature/my-feature`)
3. Follow the code style in `docs/DEVELOPMENT.md`
4. Add tests for new functionality
5. Ensure all tests pass: `ctest --preset macos-debug-test-qt`
6. Open a Pull Request

## License

MIT License — see `license.md` for details.

**Third-party libraries:**
- Qt 6: LGPL v3 (dynamically linked)
- nlohmann/json, spdlog, fmt, CURL, Google Test: MIT / BSD

## Support

- **Issues**: https://github.com/bula173/LogViewer/issues
- **Discussions**: https://github.com/bula173/LogViewer/discussions
