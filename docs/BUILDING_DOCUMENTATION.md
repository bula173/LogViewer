# Building Documentation

This document explains how to build LogViewer documentation locally and how it's automatically built on GitHub.

## Documentation Types

LogViewer provides two types of documentation:

1. **API Documentation** (Doxygen) - Generated from C++ source code comments
2. **User & Developer Guides** (Markdown) - Human-written documentation in `docs/` folder

## Building Locally

### Prerequisites

**For Doxygen (API docs):**
```bash
# macOS
brew install doxygen graphviz

# Linux
sudo apt-get install doxygen graphviz

# Windows (with Chocolatey)
choco install doxygen.install graphviz
```

**For MkDocs (Markdown docs):**
```bash
# All platforms (requires Python 3.11+)
pip install mkdocs mkdocs-material mkdocs-mermaid2-plugin
```

### Build Commands

**Doxygen API Documentation:**
```bash
# From project root
doxygen Doxyfile

# Output will be in docs/html/
# Open docs/html/index.html in your browser
```

**MkDocs Documentation Site:**
```bash
# From project root
mkdocs build

# Output will be in site/
# Open site/index.html in your browser

# Or serve locally with live reload:
mkdocs serve
# Visit http://localhost:8000
```

**CMake Integration:**
```bash
# Build Doxygen docs through CMake (if Doxygen is found)
cmake --build build-debug --target doc
```

## Automatic GitHub Builds

Documentation is automatically built on GitHub Actions when:

- Code is pushed to `main`, `qt`, or `analysis` branches
- Pull requests modify documentation or source files
- Manually triggered via workflow dispatch

### Workflow: `.github/workflows/documentation.yml`

The workflow performs:

1. **Doxygen Build** - Generates API documentation from source code
2. **MkDocs Build** - Generates user/developer documentation site
3. **Artifact Upload** - Stores generated docs for download
4. **GitHub Pages Deployment** - Deploys to `gh-pages` branch (main branch only)

### Viewing Built Documentation

**During Development:**
- Check workflow run artifacts on GitHub Actions tab
- Download `doxygen-html` or `mkdocs-site` artifacts

**Published Documentation (main branch):**
- **Live Site**: https://bula173.github.io/LogViewer/
- **API Reference**: https://bula173.github.io/LogViewer/api/

## Documentation Structure

```
docs/
├── *.md                           # User & Developer guides
├── html/                          # Doxygen output (gitignored)
├── ARCHITECTURE.md                # System architecture
├── AI_ANALYSIS_GUIDE.md          # AI features guide
├── DEVELOPMENT.md                 # Developer guide
├── UI_IMPROVEMENTS.md            # UI documentation
├── BUILD_STATUS.md               # Build system status
├── CMAKE_PRESETS_INTEGRATION.md  # CMake presets
├── CONFIGURABLE_TYPE_FILTER.md   # Type filter feature
└── ...                           # Other technical docs

src/
├── dox/                          # Doxygen-specific documentation
│   ├── overview.dox              # Project overview
│   └── architecture.dox          # Architecture diagrams
└── **/*.hpp                      # Inline API documentation
```

## Writing Documentation

### Doxygen (API Documentation)

Use Javadoc-style comments in header files:

```cpp
/**
 * @file EventsContainer.hpp
 * @brief Thread-safe container for log events
 * @author LogViewer Contributors
 */

/**
 * @class EventsContainer
 * @brief High-performance container for storing and retrieving log events
 *
 * This class provides thread-safe operations for adding and accessing
 * log events with O(1) access time.
 *
 * @par Thread Safety
 * All methods are thread-safe. Multiple threads can add events
 * concurrently.
 *
 * @see LogEvent, EventsTableModel
 */
class EventsContainer {
public:
    /**
     * @brief Add a new log event (thread-safe)
     * @param event Event to add (moved)
     * @throws std::runtime_error if container is full
     */
    void AddEvent(LogEvent&& event);
};
```

### Markdown (User Documentation)

Follow these conventions:

- Use clear headings (H1 for title, H2 for sections, H3 for subsections)
- Include code examples with language hints
- Add platform-specific instructions when needed
- Use tables for comparisons
- Include command examples for common tasks

**Example:**
````markdown
## Building on macOS

### Prerequisites

Install Qt 6:
```bash
brew install qt@6
```

### Build Commands

```bash
cmake --preset macos-debug
cmake --build --preset macos-debug-build
```
````

## MkDocs Configuration

The MkDocs configuration is generated in the GitHub workflow, but you can create a permanent `mkdocs.yml`:

```yaml
site_name: LogViewer Documentation
site_description: A modern log viewer with AI-assisted analysis
repo_url: https://github.com/bula173/LogViewer

theme:
  name: material
  palette:
    - scheme: default
      primary: indigo
      toggle:
        icon: material/brightness-7
        name: Switch to dark mode
    - scheme: slate
      primary: indigo
      toggle:
        icon: material/brightness-4
        name: Switch to light mode

nav:
  - Home: README.md
  - User Guide:
    - Getting Started: docs/DEVELOPMENT.md
    - AI Analysis: docs/AI_ANALYSIS_GUIDE.md
  - Developer Guide:
    - Architecture: docs/ARCHITECTURE.md
    - Development: docs/DEVELOPMENT.md
```

## Troubleshooting

### Doxygen Issues

**"dot not found" warning:**
```bash
# Install graphviz
brew install graphviz  # macOS
sudo apt-get install graphviz  # Linux
```

**Empty or missing documentation:**
- Check `INPUT` paths in Doxyfile
- Verify source files have Doxygen comments
- Look for warnings in Doxygen output

### MkDocs Issues

**"Config file not found":**
- Ensure `mkdocs.yml` exists in project root
- Or let the GitHub workflow generate it automatically

**"Page not found" errors:**
- Check that all files referenced in `nav:` exist
- Verify relative paths in `mkdocs.yml`

**Python package errors:**
```bash
pip install --upgrade mkdocs mkdocs-material
```

## Contributing to Documentation

1. **API Documentation**: Add/update Doxygen comments in `.hpp` files
2. **User Guides**: Edit/create `.md` files in `docs/` folder
3. **Test Locally**: Build and verify documentation before committing
4. **Submit PR**: Documentation changes trigger automatic builds

### Documentation Review Checklist

- [ ] Doxygen comments added for new public APIs
- [ ] Code examples tested and working
- [ ] Links are valid and not broken
- [ ] Screenshots updated if UI changed
- [ ] Spelling and grammar checked
- [ ] Platform-specific instructions included where needed
- [ ] Local build successful (`doxygen Doxyfile` and `mkdocs build`)

## Resources

- [Doxygen Manual](https://www.doxygen.nl/manual/)
- [MkDocs Documentation](https://www.mkdocs.org/)
- [Material for MkDocs](https://squidfunk.github.io/mkdocs-material/)
- [GitHub Pages Documentation](https://docs.github.com/en/pages)
