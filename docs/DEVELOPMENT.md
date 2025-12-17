# LogViewer Development Guide

## Getting Started

### Prerequisites

#### Required
- **CMake** 3.25+
- **C++20 Compiler**: Clang 15+, GCC 11+, or MSVC 2019+

#### GUI Framework (Choose One)

**Qt 6** (Default - includes AI features):
- **Qt 6**: 6.5 or higher
  - Windows: Download Qt installer from qt.io or use vcpkg
  - macOS: `brew install qt@6`
  - Linux: `sudo apt-get install qt6-base-dev` (Ubuntu/Debian)
- **CURL**: For AI provider HTTP requests
  - Windows: Available through vcpkg
  - macOS: Included by default
  - Linux: `sudo apt-get install libcurl4-openssl-dev`

**wxWidgets** (Traditional native UI):
- **wxWidgets**: 3.2 or higher
  - Windows: Download from wxwidgets.org or use vcpkg
  - macOS: `brew install wxwidgets`
  - Linux: `sudo apt-get install libwxgtk3.2-dev` (Ubuntu/Debian)
  - Windows (MSYS2): `pacman -S mingw-w64-x86_64-wxwidgets3.2-msw`

#### Optional but Recommended
- **Ninja** build system (recommended for faster builds)
  - Windows: Available through package managers (choco, scoop)
  - macOS: `brew install ninja`
  - Linux: `sudo apt-get install ninja-build`
- **ccache** (optional, for faster rebuilds)
- **clang-format**: For code formatting
- **clang-tidy**: For static analysis
- **Doxygen**: For documentation generation

### Quick Start

```bash
# Clone repository
git clone https://github.com/bula173/LogViewer.git
cd LogViewer

# Configure and build (Debug) - macOS example with Qt (default)
cmake --preset macos-debug-qt
cmake --build --preset macos-debug-build-qt

# Or configure with wxWidgets
cmake --preset macos-debug-wx
cmake --build --preset macos-debug-build-wx

# Run tests
ctest --preset macos-debug-test-qt

# Run application from build directory
./build/macos-debug-qt/bin/LogViewerQt.app/Contents/MacOS/LogViewerQt
# or use the script
./scripts/run_debug.sh
```

**Platform-specific presets:**
- macOS Qt: `macos-debug-qt`, `macos-release-qt`
- macOS wxWidgets: `macos-debug-wx`, `macos-release-wx`
- Windows (MSYS2) Qt: `windows-msys-debug-qt`, `windows-msys-release-qt`
- Windows (MSYS2) wxWidgets: `windows-msys-debug-wx`, `windows-msys-release-wx`
- Linux Qt: `linux-debug-qt`, `linux-release-qt`
- Linux wxWidgets: `linux-debug-wx`, `linux-release-wx`

**Using VS Code Tasks (Recommended):**

The project includes pre-configured tasks accessible via `Cmd+Shift+P` (macOS) or `Ctrl+Shift+P` (Windows/Linux) > "Tasks: Run Task":

- **Build Debug** - Configure and build debug version (Qt by default)
- **Build Release** - Configure and build release version (Qt by default)
- **Run App (Debug)** - Build and run debug application
- **Test Debug** - Build and run tests
- **Static Analysis (cppcheck)** - Run static analysis
- **Package Release** - Create distributable package

**Note:** VS Code tasks use Qt by default. For wxWidgets builds, use command line with `-DGUI_FRAMEWORK=WX`.

## Build System Architecture

### Directory Structure

The build system separates build artifacts from distribution files:

```
LogViewer/
├── build/                          # Build outputs (not in git)
│   └── <preset-name>/              # e.g., macos-debug-qt
│       ├── bin/                    # Executables for development
│       │   ├── LogViewerQt.app/
│       │   └── etc/                # Config files copied here
│       ├── lib/                    # Static libraries
│       ├── plugins/                # Plugin shared libraries (.dylib/.so/.dll)
│       │   ├── ai_provider.dylib
│       │   ├── ai_provider.zip     # Plugin packages
│       │   └── *_package/          # Temporary packaging directories
│       └── CMakeFiles/             # CMake internal files
├── dist/                           # Distribution files
│   ├── packages/                   # Final installers (.dmg, .zip, .exe)
│   │   └── LogViewerQt-1.0.0-Darwin.dmg
│   └── staging/                    # Install staging (not in git)
│       ├── LogViewerQt.app/
│       ├── etc/
│       └── plugins/
└── src/                            # Source code
```

### CMake Output Directories

The build system uses standard CMake output directories:

- `CMAKE_RUNTIME_OUTPUT_DIRECTORY`: `build/<preset>/bin` - Executables
- `CMAKE_LIBRARY_OUTPUT_DIRECTORY`: `build/<preset>/lib` - Shared libraries  
- `CMAKE_ARCHIVE_OUTPUT_DIRECTORY`: `build/<preset>/lib` - Static libraries

**Development workflow:**
1. Build: `cmake --build --preset <preset>`
2. Run from build: `./build/<preset>/bin/LogViewerQt.app/...`
3. All outputs stay in `build/` directory

**Distribution workflow:**
1. Install: `cmake --install build/<preset> --prefix dist/staging`
2. Package: `cpack --config build/<preset>/CPackConfig.cmake`
3. Installers go to `dist/packages/`

### Scripts

Convenience scripts in `scripts/` directory:

- `build_debug.sh` - Build debug version
- `configure_and_build_osx.sh` - Configure and build from scratch
- `run_debug.sh` - Run debug application
- Windows equivalents: `.bat` files

All scripts updated to use new directory structure.

## Code Style Guide

### Naming Conventions

```cpp
// Classes: PascalCase
class EventsContainer { };

// Functions/Methods: camelCase
void parseData();
int getCurrentIndex();

// Member variables: m_camelCase
private:
    std::string m_fileName;
    int m_eventCount;

// Constants: UPPER_SNAKE_CASE
const int MAX_EVENTS = 1000000;

// Namespaces: lowercase
namespace parser { }
namespace filters { }

// Template parameters: PascalCase
template<typename T, typename ErrorType>
class Result { };
```

### File Organization

```cpp
// Header file: EventsContainer.hpp
/**
 * @file EventsContainer.hpp
 * @brief Brief description
 * @author Your Name
 * @date 2025
 */

#pragma once

// System includes
#include <vector>
#include <memory>

// Third-party includes
#include <nlohmann/json.hpp>
// Qt version:
#include <QMainWindow>
#include <QTableView>
// wxWidgets version:
// #include <wx/wx.h>
// #include <wx/dataview.h>

// Project includes
#include "db/LogEvent.hpp"
#include "mvc/IModel.hpp"

namespace db {

/**
 * @class EventsContainer
 * @brief Detailed description
 *
 * More details about the class, usage examples, performance characteristics.
 */
class EventsContainer {
public:
    // Constructors
    EventsContainer();
    explicit EventsContainer(size_t reserveSize);
    
    // Destructor
    virtual ~EventsContainer();
    
    // Copy/Move (Rule of 5)
    EventsContainer(const EventsContainer&) = delete;
    EventsContainer& operator=(const EventsContainer&) = delete;
    EventsContainer(EventsContainer&&) noexcept = default;
    EventsContainer& operator=(EventsContainer&&) noexcept = default;
    
    // Public interface
    void AddEvent(LogEvent&& event);
    const LogEvent& GetEvent(int index) const;
    size_t Size() const;
    
private:
    // Private methods
    void validateIndex(int index) const;
    
    // Member variables
    std::vector<LogEvent> m_data;
    mutable std::mutex m_mutex;
};

} // namespace db
```

### Documentation Standards

#### Doxygen Comments

```cpp
/**
 * @brief Adds a new event to the container.
 *
 * Appends the event to the internal storage using move semantics
 * for efficiency. The method is thread-safe and can be called
 * from multiple threads concurrently.
 *
 * @param event The LogEvent to add (moved into container)
 *
 * @throws std::bad_alloc if memory allocation fails
 *
 * @note The event is moved, so the passed object will be in a
 *       valid but unspecified state after this call.
 *
 * @par Complexity
 * O(1) amortized
 *
 * @par Example:
 * @code
 * EventsContainer container;
 * LogEvent event(1, {{"message", "Error occurred"}});
 * container.AddEvent(std::move(event));
 * // event is no longer valid here
 * @endcode
 *
 * @see GetEvent(), Size()
 */
void AddEvent(LogEvent&& event);
```

### Code Quality Checklist

#### Before Committing
- [ ] All tests pass
- [ ] No compiler warnings
- [ ] Code formatted with clang-format
- [ ] Doxygen comments for public APIs
- [ ] Thread safety documented
- [ ] Performance implications noted
- [ ] Error handling implemented
- [ ] Resource cleanup (RAII)

## Design Patterns Usage

### When to Use Each Pattern

#### Observer Pattern ✅
**Use when**: Components need loose coupling for event notification

```cpp
// Good: Parser doesn't know about UI
class XmlParser : public IDataParser {
    void ParseData(const std::filesystem::path& filepath) override {
        // ... parsing logic ...
        NotifyNewEvent(std::move(event));  // Observers updated
    }
};
```

#### Factory Pattern ✅
**Use when**: Object creation logic is complex or varies

```cpp
// Good: Centralized parser creation
auto parser = ParserFactory::CreateParser(".xml");  // Returns XmlParser
auto parser = ParserFactory::CreateParser(".json"); // Returns JsonParser
```

#### Strategy Pattern ✅
**Use when**: Algorithm needs to be swappable at runtime

```cpp
// Good: Different filter strategies
filter.SetStrategy(std::make_unique<RegexStrategy>());
filter.SetStrategy(std::make_unique<ExactMatchStrategy>());
```

#### Singleton Pattern ⚠️
**Use sparingly**: Only for truly global resources

```cpp
// Good use: Application configuration
Config& config = Config::Instance();

// Bad use: Don't make everything a singleton!
// Use dependency injection instead
```

### Qt-Specific Patterns

#### Signals and Slots
**Use when**: Components need Qt's signal/slot mechanism for event notification

```cpp
class EventsTableModel : public QAbstractTableModel {
    Q_OBJECT  // Required for signals/slots

public:
    void UpdateData(const EventsContainer& container) {
        beginResetModel();
        m_data = &container;
        endResetModel();
        emit dataChanged(index(0, 0), index(rowCount()-1, columnCount()-1));
    }

signals:
    void filterChanged(const QString& filterText);
    void eventSelected(int eventId);

public slots:
    void onConfigChanged(const Config& config) {
        // React to configuration changes
    }
};
```

#### Model/View Architecture
**Use when**: Displaying data in Qt views (tables, lists, trees)

```cpp
class EventsTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    // Required overrides
    int rowCount(const QModelIndex& parent = QModelIndex()) const override {
        return m_container ? m_container->Size() : 0;
    }
    
    int columnCount(const QModelIndex& parent = QModelIndex()) const override {
        return m_config.columns.size();
    }
    
    QVariant data(const QModelIndex& index, int role) const override {
        if (role == Qt::DisplayRole) {
            const auto& event = m_container->GetEvent(index.row());
            return QString::fromStdString(event.GetValue(m_config.columns[index.column()]));
        }
        return QVariant();
    }

private:
    const EventsContainer* m_container = nullptr;
    const Config& m_config;
};
```

#### Dock Widget System
**Use when**: Creating flexible, dockable UI panels

```cpp
class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow() {
        // Create dock widgets
        m_filtersDock = new QDockWidget(tr("Filters"), this);
        m_filtersDock->setWidget(new FiltersPanel(this));
        addDockWidget(Qt::LeftDockWidgetArea, m_filtersDock);
        
        m_detailsDock = new QDockWidget(tr("Details"), this);
        m_detailsDock->setWidget(new ItemDetailsPanel(this));
        addDockWidget(Qt::RightDockWidgetArea, m_detailsDock);
        
        // All docks are movable, floatable, and closable
    }

private:
    QDockWidget* m_filtersDock = nullptr;
    QDockWidget* m_detailsDock = nullptr;
};
```

### wxWidgets-Specific Patterns

#### Virtual List Controls
**Use when**: Displaying large datasets efficiently with wxWidgets

```cpp
class EventsVirtualListControl : public wxDataViewVirtualListCtrl {
public:
    EventsVirtualListControl(wxWindow* parent, const EventsContainer* container)
        : wxDataViewVirtualListCtrl(parent, wxID_ANY), m_container(container) {
        // Add columns
        AppendTextColumn("ID", wxDATAVIEW_CELL_INERT, 80);
        AppendTextColumn("Timestamp", wxDATAVIEW_CELL_INERT, 150);
        AppendTextColumn("Message", wxDATAVIEW_CELL_INERT, 400);
    }

    // Override to provide row count
    unsigned int GetRowCount() override {
        return m_container ? m_container->Size() : 0;
    }

    // Override to provide cell values
    void GetValueByRow(wxVariant& variant, unsigned int row,
                       unsigned int col) const override {
        if (!m_container || row >= m_container->Size()) return;
        
        const auto& event = m_container->GetEvent(row);
        switch (col) {
            case 0: variant = event.getId(); break;
            case 1: variant = event.GetValue("timestamp"); break;
            case 2: variant = event.GetValue("message"); break;
        }
    }

private:
    const EventsContainer* m_container = nullptr;
};
```

#### Event Handling
**Use when**: Responding to user interactions in wxWidgets

```cpp
class MainWindow : public wxFrame {
public:
    MainWindow() : wxFrame(nullptr, wxID_ANY, "LogViewer") {
        // Bind events
        Bind(wxEVT_MENU, &MainWindow::OnOpen, this, wxID_OPEN);
        Bind(wxEVT_MENU, &MainWindow::OnExit, this, wxID_EXIT);
        
        // Custom events
        m_eventsListCtrl->Bind(wxEVT_DATAVIEW_SELECTION_CHANGED,
            &MainWindow::OnEventSelected, this);
    }

private:
    void OnOpen(wxCommandEvent& event) {
        wxFileDialog dialog(this, _("Open Log File"),
            wxEmptyString, wxEmptyString,
            "XML files (*.xml)|*.xml|All files (*.*)|*.*",
            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        
        if (dialog.ShowModal() == wxID_OK) {
            LoadLogFile(dialog.GetPath().ToStdString());
        }
    }
    
    void OnEventSelected(wxDataViewEvent& event) {
        int row = m_eventsListCtrl->GetSelectedRow();
        if (row >= 0) {
            DisplayEventDetails(row);
        }
    }
    
    wxDataViewVirtualListCtrl* m_eventsListCtrl = nullptr;
};
```

#### wxWidgets Type Conversions
**Use when**: Converting between std::string and wxString

```cpp
#include "util/WxWidgetsUtils.hpp"

// String conversions
std::string stdStr = "Hello";
wxString wxStr = wxString::FromUTF8(stdStr);
std::string backToStd = wxStr.ToStdString();

// Safe numeric conversions
size_t count = 1000000;
long wxCount = util::ToLong(count);  // Safe conversion with overflow check
int colIndex = util::ToInt(count);   // Safe conversion with bounds check
```

## Error Handling

### Error Categories

```cpp
namespace error {

// Base class for all application errors
class Error : public std::exception {
public:
    explicit Error(std::string message) : m_message(std::move(message)) {}
    const char* what() const noexcept override { return m_message.c_str(); }
    
private:
    std::string m_message;
};

// Specific error types
class ConfigError : public Error {
    using Error::Error;
};

class ParseError : public Error {
    using Error::Error;
};

class FileError : public Error {
    using Error::Error;
};

} // namespace error
```

### Error Handling Guidelines

```cpp
// ❌ Bad: Catching everything
try {
    DoSomething();
} catch (...) {
    // What went wrong? Can't tell!
}

// ✅ Good: Specific error handling
try {
    config.Load("config.json");
} catch (const error::ConfigError& e) {
    util::Logger::Error("Configuration error: {}", e.what());
    // Provide default configuration
    config.UseDefaults();
} catch (const error::FileError& e) {
    util::Logger::Error("File error: {}", e.what());
    // Show file picker dialog
    ShowFilePickerDialog();
}

// ✅ Best: Result<T> for expected failures
Result<Config, ConfigError> LoadConfig(const std::string& path) {
    if (!std::filesystem::exists(path)) {
        return Result::Err(ConfigError("File not found"));
    }
    
    Config config = /* ... */;
    return Result::Ok(std::move(config));
}

// Usage
auto result = LoadConfig("config.json");
if (result.isOk()) {
    Config config = result.unwrap();
    // use config
} else {
    util::Logger::Error("Failed to load config: {}", result.error().what());
}
```

## Thread Safety

### Guidelines

1. **Document thread safety** of every class
2. **Prefer immutability** when possible
3. **Use RAII** for lock management
4. **Avoid deadlocks** with lock ordering

```cpp
/**
 * @class EventsContainer
 * @brief Thread-safe container for log events
 *
 * @par Thread Safety
 * All methods are thread-safe. Multiple threads can add events
 * concurrently. Read operations are lock-free for const methods.
 */
class EventsContainer {
public:
    /// Thread-safe addition (locks internally)
    void AddEvent(LogEvent&& event) {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_data.push_back(std::move(event));
    }
    
    /// Thread-safe read (shared lock)
    const LogEvent& GetEvent(int index) const {
        std::shared_lock<std::shared_mutex> lock(m_mutex);
        return m_data.at(index);
    }
    
    /// Lock-free size query
    size_t Size() const noexcept {
        return m_data.size();  // atomic on most platforms
    }
    
private:
    mutable std::shared_mutex m_mutex;
    std::vector<LogEvent> m_data;
};
```

## Performance Best Practices

### Memory Management

```cpp
// ❌ Bad: Unnecessary copies
void ProcessEvents(EventsContainer container) {
    // Entire container copied!
}

// ✅ Good: Pass by const reference
void ProcessEvents(const EventsContainer& container) {
    // No copy, read-only access
}

// ✅ Better: Use move semantics when transferring ownership
void TakeOwnership(EventsContainer&& container) {
    m_container = std::move(container);
}
```

### String Handling

```cpp
// ❌ Bad: String concatenation in loop
std::string result;
for (const auto& item : items) {
    result += item + ", ";  // Multiple reallocations!
}

// ✅ Good: Reserve capacity
std::string result;
result.reserve(estimatedSize);
for (const auto& item : items) {
    result += item + ", ";
}

// ✅ Best: Use string stream for complex formatting
std::ostringstream oss;
for (const auto& item : items) {
    oss << item << ", ";
}
std::string result = oss.str();
```

### Container Operations

```cpp
// ❌ Bad: Reallocations during growth
std::vector<LogEvent> events;
for (int i = 0; i < 1000000; ++i) {
    events.push_back(CreateEvent(i));  // Many reallocations!
}

// ✅ Good: Reserve capacity upfront
std::vector<LogEvent> events;
events.reserve(1000000);
for (int i = 0; i < 1000000; ++i) {
    events.push_back(CreateEvent(i));
}

// ✅ Best: Use emplace_back to construct in-place
events.reserve(1000000);
for (int i = 0; i < 1000000; ++i) {
    events.emplace_back(i, CreateItems(i));
}
```

## AI Integration Development

### Adding a New AI Provider

The application uses a factory pattern for AI providers. To add a new provider:

#### 1. Implement IAIService Interface

```cpp
// src/plugins/ai/MyNewAIClient.hpp
#pragma once

#include "IAIService.hpp"
#include <string>

namespace ai {

class MyNewAIClient : public IAIService {
public:
    MyNewAIClient() = default;
    ~MyNewAIClient() override = default;

    std::string Chat(
        const std::string& systemPrompt,
        const std::string& userPrompt,
        const std::string& model) override;

    std::vector<std::string> ListModels() override;
};

} // namespace ai
```

#### 2. Implement Chat Method with CURL

```cpp
// src/plugins/ai/MyNewAIClient.cpp
#include "MyNewAIClient.hpp"
#include "config/Config.hpp"
#include <curl/curl.h>
#include <nlohmann/json.hpp>

namespace ai {

std::string MyNewAIClient::Chat(
    const std::string& systemPrompt,
    const std::string& userPrompt,
    const std::string& model) {
    
    CURL* curl = curl_easy_init();
    if (!curl) {
        throw std::runtime_error("Failed to initialize CURL");
    }

    // Get timeout from config (range: 30-3600 seconds)
    const auto& config = config::GetConfig();
    curl_easy_setopt(curl, CURLOPT_TIMEOUT,
        static_cast<long>(config.aiTimeoutSeconds));

    // Prepare request JSON
    nlohmann::json request = {
        {"model", model},
        {"messages", nlohmann::json::array({
            {{"role", "system"}, {"content", systemPrompt}},
            {{"role", "user"}, {"content", userPrompt}}
        })}
    };

    std::string requestBody = request.dump();
    std::string response;

    // Set CURL options
    curl_easy_setopt(curl, CURLOPT_URL, "https://api.myprovider.com/v1/chat");
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, requestBody.c_str());
    
    // Add headers
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers, "Content-Type: application/json");
    std::string authHeader = "Authorization: Bearer " + config.aiApiKey;
    headers = curl_slist_append(headers, authHeader.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

    // Write callback
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, [](void* contents, size_t size,
        size_t nmemb, std::string* s) -> size_t {
        s->append((char*)contents, size * nmemb);
        return size * nmemb;
    });
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);

    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        throw std::runtime_error("CURL error: " +
            std::string(curl_easy_strerror(res)));
    }

    // Parse response
    auto jsonResponse = nlohmann::json::parse(response);
    return jsonResponse["choices"][0]["message"]["content"].get<std::string>();
}

std::vector<std::string> MyNewAIClient::ListModels() {
    // Implement model listing
    return {"my-model-1", "my-model-2"};
}

} // namespace ai
```

#### 3. Register in AIServiceFactory

```cpp
// src/plugins/ai/AIServiceFactory.cpp
#include "MyNewAIClient.hpp"

std::unique_ptr<IAIService> AIServiceFactory::CreateService(
    const std::string& provider) {
    
    if (provider == "ollama" || provider == "lmstudio") {
        return std::make_unique<OllamaClient>();
    } else if (provider == "openai") {
        return std::make_unique<OpenAIClient>();
    } else if (provider == "mynewai") {  // Add your provider
        return std::make_unique<MyNewAIClient>();
    }
    
    throw std::runtime_error("Unknown AI provider: " + provider);
}
```

#### 4. Add to Configuration

```json
// etc/default_config.json
{
  "aiConfig": {
    "provider": "mynewai",
    "apiKey": "your-api-key-here",
    "defaultModel": "my-model-1",
    "timeoutSeconds": 300
  }
}
```

### AI Provider Guidelines

#### Timeout Configuration
- **Always use** `config.aiTimeoutSeconds` from Config
- **Valid range**: 30-3600 seconds (enforced by UI)
- **Default**: 300 seconds (5 minutes)
- **Why**: Slow machines or large analyses need more time

```cpp
// ✅ Good: Use configurable timeout
const auto& config = config::GetConfig();
curl_easy_setopt(curl, CURLOPT_TIMEOUT,
    static_cast<long>(config.aiTimeoutSeconds));

// ❌ Bad: Hardcoded timeout
curl_easy_setopt(curl, CURLOPT_TIMEOUT, 300L);
```

#### Error Handling
- Throw descriptive exceptions for CURL errors
- Throw exceptions for HTTP error status codes
- Throw exceptions for malformed JSON responses
- All exceptions caught and displayed in UI

#### Thread Safety
- AI requests run in background threads via `QtConcurrent::run()`
- Don't access UI directly from AI threads
- Use signals/slots to update UI from background threads

```cpp
// In UI class
void AIAnalysisPanel::OnAnalyze() {
    QString prompt = m_promptEdit->toPlainText();
    
    // Run in background thread
    auto future = QtConcurrent::run([this, prompt]() {
        auto aiService = ai::AIServiceFactory::CreateService(
            config::GetConfig().aiProvider);
        return aiService->Chat(systemPrompt, prompt.toStdString(), model);
    });
    
    // Update UI when complete (on main thread)
    auto watcher = new QFutureWatcher<std::string>(this);
    connect(watcher, &QFutureWatcher<std::string>::finished, this, [this, watcher]() {
        try {
            QString result = QString::fromStdString(watcher->result());
            m_responseEdit->setPlainText(result);
        } catch (const std::exception& e) {
            QMessageBox::critical(this, tr("Error"), e.what());
        }
        watcher->deleteLater();
    });
    watcher->setFuture(future);
}
```

### Filter-Aware Analysis

AI analysis respects the configurable `typeFilterField`:

```cpp
// Get filter field from config (e.g., "level", "type", "severity")
const auto& config = config::GetConfig();
std::string filterField = config.typeFilterField;

// When formatting events for AI analysis
for (const auto& event : events) {
    std::string filterValue = event.GetValue(filterField);
    // Include in analysis prompt
}
```

This allows analysis of logs that use different fields for categorization:
- `"type"` - Traditional event types
- `"level"` - Log levels (ERROR, WARN, INFO, DEBUG)
- `"severity"` - Severity levels (CRITICAL, HIGH, MEDIUM, LOW)
- `"priority"` - Priority levels (P0, P1, P2, P3)

## Testing

### Unit Test Structure

Unit tests are framework-agnostic and test the core business logic, not UI code.

```cpp
#include <gtest/gtest.h>
#include "db/EventsContainer.hpp"

namespace db::test {

// Test fixture for common setup
class EventsContainerTest : public ::testing::Test {
protected:
    void SetUp() override {
        container = std::make_unique<EventsContainer>();
    }
    
    void TearDown() override {
        container.reset();
    }
    
    std::unique_ptr<EventsContainer> container;
};

// Individual tests
TEST_F(EventsContainerTest, AddSingleEvent) {
    LogEvent event(1, {{"key", "value"}});
    container->AddEvent(std::move(event));
    
    EXPECT_EQ(container->Size(), 1);
    EXPECT_EQ(container->GetEvent(0).getId(), 1);
}

TEST_F(EventsContainerTest, ThrowsOnInvalidIndex) {
    EXPECT_THROW(container->GetEvent(0), std::out_of_range);
}

TEST_F(EventsContainerTest, MultiThreadedAddition) {
    constexpr int NUM_THREADS = 10;
    constexpr int EVENTS_PER_THREAD = 1000;
    
    std::vector<std::thread> threads;
    for (int t = 0; t < NUM_THREADS; ++t) {
        threads.emplace_back([this, t]() {
            for (int i = 0; i < EVENTS_PER_THREAD; ++i) {
                LogEvent event(t * EVENTS_PER_THREAD + i, {});
                container->AddEvent(std::move(event));
            }
        });
    }
    
    for (auto& thread : threads) {
        thread.join();
    }
    
    EXPECT_EQ(container->Size(), NUM_THREADS * EVENTS_PER_THREAD);
}

} // namespace db::test
```

### Mock Objects

```cpp
// Mock observer for testing parsers
class MockObserver : public IDataParserObserver {
public:
    MOCK_METHOD(void, ProgressUpdated, (), (override));
    MOCK_METHOD(void, NewEventFound, (LogEvent&&), (override));
    MOCK_METHOD(void, NewEventBatchFound,
        (std::vector<std::pair<int, LogEvent::EventItems>>&&), (override));
};

// Usage in tests
TEST(XmlParserTest, NotifiesObservers) {
    MockObserver observer;
    XmlParser parser;
    parser.RegisterObserver(&observer);
    
    EXPECT_CALL(observer, NewEventFound(testing::_))
        .Times(testing::AtLeast(1));
    
    parser.ParseData("test.xml");
}
```

## Debugging Tips

### Logging Levels

```cpp
// Use appropriate log levels
util::Logger::Trace("Entering function with param={}", param);  // Very verbose
util::Logger::Debug("Processing event id={}", id);              // Development
util::Logger::Info("File loaded: {}", filename);                // Normal operation
util::Logger::Warn("Config missing, using defaults");           // Potential issues
util::Logger::Error("Failed to parse: {}", error);              // Errors
util::Logger::Critical("Out of memory!");                       // Fatal errors
```

### Common Issues

**Issue**: Crash on vector access
```cpp
// Debug build: Check assertions
assert(index < m_data.size() && "Index out of range");

// Release build: Use at() for bounds checking
return m_data.at(index);  // Throws std::out_of_range
```

**Issue**: Memory leaks
```cpp
// Use smart pointers!
std::unique_ptr<Parser> parser = ParserFactory::CreateParser(".xml");
// Automatically deleted when out of scope

// Or shared ownership
std::shared_ptr<Config> config = std::make_shared<Config>();
```

**Issue**: Deadlocks
```cpp
// Always acquire locks in same order
class A {
    void DoWork() {
        std::lock_guard<std::mutex> lock1(m_mutex);
        std::lock_guard<std::mutex> lock2(other.m_mutex);  // ❌ Can deadlock!
    }
};

// Solution: Use std::scoped_lock for multiple mutexes
void DoWork() {
    std::scoped_lock lock(m_mutex, other.m_mutex);  // ✅ Deadlock-free
}
```

## Git Workflow

### Branch Naming
- `feature/add-json-parser` - New features
- `bugfix/fix-crash-on-empty-file` - Bug fixes
- `refactor/improve-filter-performance` - Code improvements
- `docs/update-architecture` - Documentation

### Commit Messages
```
feat: Add JSON parser support

- Implement JsonParser class following IDataParser interface
- Add tests for JSON parsing edge cases
- Update ParserFactory to handle .json files

Closes #123
```

### Pull Request Template
```markdown
## Description
Brief description of changes

## Type of Change
- [ ] Bug fix
- [ ] New feature
- [ ] Breaking change
- [ ] Documentation update

## Testing
- [ ] Unit tests added
- [ ] Integration tests pass
- [ ] Manual testing performed

## Checklist
- [ ] Code follows style guide
- [ ] Documentation updated
- [ ] No new warnings
- [ ] Tests pass locally
```

## Useful Commands

```bash
# Build with specific framework
cmake --preset macos-debug -DGUI_FRAMEWORK=QT   # Qt build
cmake --preset macos-debug -DGUI_FRAMEWORK=WX   # wxWidgets build

# Format all code
find src -name "*.cpp" -o -name "*.hpp" | xargs clang-format -i

# Run static analysis
clang-tidy src/**/*.cpp -- -I src

# Generate documentation
doxygen Doxyfile

# Run with sanitizers
cmake --preset windows-msys-debug -DENABLE_SANITIZER=address
cmake --build --preset windows-msys-debug-build
./dist/Debug/LogViewer

# Profile performance
perf record ./dist/Release/LogViewer
perf report

# Memory profiling
valgrind --tool=massif ./dist/Debug/LogViewer
```

## Resources

### C++ and General
- [C++ Core Guidelines](https://isocpp.github.io/CppCoreGuidelines/)
- [Effective Modern C++](https://www.oreilly.com/library/view/effective-modern-c/9781491908419/)
- [Google C++ Style Guide](https://google.github.io/styleguide/cppguide.html)
- [CMake Documentation](https://cmake.org/documentation/)

### Qt Framework
- [Qt 6 Documentation](https://doc.qt.io/qt-6/)
- [Qt Model/View Programming](https://doc.qt.io/qt-6/model-view-programming.html)
- [Qt Signals and Slots](https://doc.qt.io/qt-6/signalsandslots.html)
- [QMainWindow and Dock Widgets](https://doc.qt.io/qt-6/qmainwindow.html)

### wxWidgets Framework
- [wxWidgets Documentation](https://docs.wxwidgets.org/)
- [wxDataViewCtrl](https://docs.wxwidgets.org/3.2/classwx_data_view_ctrl.html)
- [Event Handling](https://docs.wxwidgets.org/3.2/overview_events.html)
- [Virtual List Controls](https://docs.wxwidgets.org/3.2/overview_dataview.html)

### AI Provider APIs
- [Ollama API Documentation](https://github.com/ollama/ollama/blob/main/docs/api.md)
- [OpenAI API Reference](https://platform.openai.com/docs/api-reference)
- [Anthropic API Documentation](https://docs.anthropic.com/claude/reference)
- [Google Gemini API](https://ai.google.dev/docs)
- [xAI API Documentation](https://docs.x.ai/)
