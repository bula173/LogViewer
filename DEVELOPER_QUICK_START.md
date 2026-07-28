# LogViewer: Developer Quick Reference Guide

**For:** Implementing architectural fixes and AI enhancements  
**Last Updated:** 2026-06-15

---

## Quick Navigation

### I Just Want To... 🎯
- **Fix the TODO in MainWindow** → [#1](#1-fix-service-interface-todo)
- **Make Gemma actually work** → [#2](#2-enable-actual-gemma-inference)
- **Add anomaly detection** → [#3](#3-implement-anomaly-detection)
- **Build filter assistant** → [#4](#4-build-filter-assistant)
- **See all 7 issues** → See `ARCHITECTURE_ANALYSIS.md`

---

## Issue #1: Fix Service Interface TODO

**File:** `src/application/ui/qt/MainWindow.hpp:50`

**Current (Bad):**
```cpp
std::shared_ptr<ai::IAIService> m_pluginService;  // TODO: Generalize beyond AI
```

**Target (Good):**
```cpp
std::shared_ptr<services::IService> m_currentService;
```

**Implementation Steps:**

### Step A: Create base service interface
```bash
# Create new file
touch src/application/services/IService.hpp
```

```cpp
// src/application/services/IService.hpp
#pragma once
#include <string>
#include <memory>

namespace services {

class IService {
public:
    virtual ~IService() = default;
    virtual std::string GetServiceId() const = 0;
    virtual std::string GetServiceType() const = 0;  // "ai", "analyzer", etc
};

}  // namespace services
```

### Step B: Update IAIService
```cpp
// src/application/ai/IAIService.hpp (MODIFY)
#pragma once
#include "../services/IService.hpp"  // NEW: Add this include
#include <string>
#include <functional>

namespace ai {

class IAIService : public services::IService {  // MODIFY: Inherit from IService
public:
    virtual ~IAIService() = default;
    
    // IService implementation
    std::string GetServiceType() const override { return "ai"; }
    
    // AI-specific methods
    virtual std::string SendPrompt(const std::string& prompt,
        std::function<void(const std::string&)> callback = nullptr) = 0;
    virtual bool IsAvailable() const = 0;
    virtual std::string GetModelName() const = 0;
    virtual void SetModelName(const std::string& modelName) = 0;
    virtual std::string GetProviderName() const = 0;
};

}  // namespace ai
```

### Step C: Update MainWindow
```cpp
// src/application/ui/qt/MainWindow.hpp (MODIFY)
#pragma once
// ... existing includes ...
#include "IService.hpp"  // CHANGE: was #include "IAIService.hpp"
#include <memory>

// ... class definition ...

private:
    // OLD: std::shared_ptr<ai::IAIService> m_pluginService;  // TODO: ...
    // NEW:
    std::shared_ptr<services::IService> m_currentService;
    
    /// Helper to cast to AI service if applicable
    std::shared_ptr<ai::IAIService> GetAIService() const {
        return std::dynamic_pointer_cast<ai::IAIService>(m_currentService);
    }
```

### Step D: Update usages in MainWindow.cpp
```cpp
// src/application/ui/qt/MainWindow.cpp (FIND & REPLACE)

// OLD:
// m_pluginService->SendPrompt(...)

// NEW (if optional, check first):
if (auto aiService = GetAIService()) {
    aiService->SendPrompt(...);
} else {
    // Handle missing AI service
}

// NEW (if required):
auto aiService = GetAIService();
if (!aiService) {
    showError("AI service not available");
    return;
}
aiService->SendPrompt(...);
```

### Step E: Update CMakeLists.txt (if needed)
```cmake
# src/CMakeLists.txt
target_sources(application_core PUBLIC
    # ... existing files ...
    application/services/IService.hpp  # ADD THIS LINE
)
```

### Step F: Test
```bash
cd /Users/Marcin/workspace/cpp/LogViewer
cmake --build build/macos-debug-qt --target LogViewer_tests -j4
ctest --preset macos-debug-test --verbose
```

**Verification:**
- [ ] MainWindow.hpp compiles
- [ ] No linker errors
- [ ] Tests still pass
- [ ] TODO comment is gone

**Time:** ~30 minutes

---

## Issue #2: Enable Actual Gemma Inference

**Why:** Currently loads model but never runs it—returns only heuristics.

**File:** `src/application/ai/GemmaInferenceEngine.cpp:134`

**Current (Broken):**
```cpp
util::Logger::Info("[Gemma] Using heuristic analysis with LLM model ready...");
// Never actually calls llama inference
```

**Target:** Actually run inference on sample messages

### Step A: Create inference utility
```bash
touch src/application/ai/GemmaInference.hpp
touch src/application/ai/GemmaInference.cpp
```

**src/application/ai/GemmaInference.hpp:**
```cpp
#pragma once
#include <string>
#include <set>
#include <llama.h>

namespace ai {

class GemmaInference {
public:
    /// Run LLM to extract actors from messages
    static std::string ExtractActors(
        llama_context* ctx,
        const std::set<std::string>& messages);
    
    /// Parse JSON response
    static std::set<std::string> ParseActorsFromResponse(
        const std::string& response);
    
private:
    static std::string BuildPrompt(const std::set<std::string>& messages);
    static std::string RunInference(
        llama_context* ctx,
        const std::string& prompt,
        int maxTokens = 512);
};

}  // namespace ai
```

**src/application/ai/GemmaInference.cpp:**
```cpp
#include "GemmaInference.hpp"
#include "Logger.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <llama.h>

namespace ai {

std::string GemmaInference::ExtractActors(
    llama_context* ctx,
    const std::set<std::string>& messages)
{
    if (!ctx) return "";
    
    std::string prompt = BuildPrompt(messages);
    return RunInference(ctx, prompt, 512);
}

std::string GemmaInference::BuildPrompt(
    const std::set<std::string>& messages)
{
    std::ostringstream oss;
    oss << "Extract all service names from these logs. Return JSON:\n";
    oss << R"({"actors": ["service1", ...]})"; oss << "\n\nLogs:\n";
    
    for (const auto& msg : messages) {
        oss << "- " << msg << "\n";
    }
    
    return oss.str();
}

std::string GemmaInference::RunInference(
    llama_context* ctx,
    const std::string& prompt,
    int maxTokens)
{
    // Tokenize
    std::vector<llama_token> tokens = llama_tokenize(
        llama_get_model(ctx),
        prompt.c_str(), prompt.length(),
        false);
    
    if (tokens.empty()) {
        util::Logger::Warn("[Gemma] Tokenization failed");
        return "";
    }
    
    // Run inference
    std::string result;
    // [Simplified: see IMPLEMENTATION_ROADMAP.md for full details]
    
    return result;
}

std::set<std::string> GemmaInference::ParseActorsFromResponse(
    const std::string& response)
{
    std::set<std::string> actors;
    
    try {
        auto json = nlohmann::json::parse(response);
        if (json.contains("actors")) {
            for (const auto& actor : json["actors"]) {
                if (actor.is_string()) {
                    actors.insert(actor.get<std::string>());
                }
            }
        }
    } catch (...) {
        // Parse error
    }
    
    return actors;
}

}  // namespace ai
```

### Step B: Update GemmaInferenceEngine
```cpp
// src/application/ai/GemmaInferenceEngine.cpp (MODIFY)

#include "GemmaInference.hpp"  // ADD

GemmaActorResult GemmaInferenceEngine::ExtractActors(
    const std::set<std::string>& sampleMessages)
{
    GemmaActorResult result;
    
    if (!IsAvailable()) {
        result.error = "Gemma model not available";
        result.confidence = 0;
        return result;
    }
    
    // TRY: Actual inference
    try {
        std::string output = GemmaInference::ExtractActors(
            s_impl->ctx, sampleMessages);
        
        if (!output.empty()) {
            result.actors = GemmaInference::ParseActorsFromResponse(output);
            result.confidence = 85;  // LLM confidence
            util::Logger::Info("[Gemma] Extracted {} actors via LLM",
                             result.actors.size());
            return result;
        }
    }
    catch (const std::exception& e) {
        util::Logger::Debug("[Gemma] Inference failed: {}, using heuristic",
                          e.what());
    }
    
    // FALLBACK: Heuristic if inference fails
    result = HeuristicActorExtraction(sampleMessages);
    return result;
}
```

### Step C: Update CMakeLists
```cmake
# src/application/ai/CMakeLists.txt (or src/CMakeLists.txt)
target_sources(application_core PUBLIC
    # ... existing files ...
    application/ai/GemmaInference.hpp
    application/ai/GemmaInference.cpp
)
```

### Step D: Test
```bash
cd /Users/Marcin/workspace/cpp/LogViewer
cmake --build build/macos-debug-qt -j4
# Should compile without errors
```

**Verification:**
- [ ] Compiles without errors
- [ ] Gemma model loads successfully
- [ ] Inference actually runs (check logs for "Extracted X actors via LLM")
- [ ] Falls back to heuristic if inference fails

**Time:** ~6 hours (4 hours coding + 2 hours testing)

---

## Issue #3: Implement Anomaly Detection

**Why:** Users need "find unusual patterns" feature

**Files to Create:**
- `src/application/ai/AnomalyDetector.hpp`
- `src/application/ai/AnomalyDetector.cpp`

**src/application/ai/AnomalyDetector.hpp:**
```cpp
#pragma once
#include "IAIService.hpp"
#include "IModel.hpp"
#include <vector>
#include <memory>

namespace ai {

enum class AnomalyType {
    ErrorSpike,          // Sudden error increase
    LatencyAnomaly,      // Slow responses
    UnusualActor,        // Unexpected service
    ProtocolViolation,   // Out of order
    ResourceExhaustion,  // Memory/CPU limits
    SecurityAnomaly,     // Auth failures, etc
};

struct AnomalyReport {
    AnomalyType type;
    float severityScore {0.0f};       // 0.0-1.0
    std::vector<size_t> affectedEventIndices;
    std::string explanation;
    std::vector<std::string> recommendations;
    
    std::string GetTypeString() const;  // For UI display
};

class AnomalyDetector {
public:
    explicit AnomalyDetector(std::shared_ptr<IAIService> aiService);
    
    /// Find anomalies in event window
    std::vector<AnomalyReport> Detect(
        const mvc::IModel& events,
        size_t windowSize = 1000);
    
    /// Correlate related anomalies
    std::vector<std::vector<AnomalyReport>> FindCorrelated(
        const std::vector<AnomalyReport>& anomalies) const;
    
    /// Explain root cause
    std::string AnalyzeRootCause(
        const AnomalyReport& anomaly,
        const mvc::IModel& events);
    
    bool IsReady() const { 
        return m_aiService && m_aiService->IsAvailable(); 
    }
    
private:
    std::shared_ptr<IAIService> m_aiService;
    
    std::string FormatEventsForAnalysis(
        const mvc::IModel& events,
        const std::vector<size_t>& indices);
    
    std::vector<AnomalyReport> ParseAnomalies(
        const std::string& llmResponse,
        const mvc::IModel& events);
};

}  // namespace ai
```

**For full implementation, see:** `IMPLEMENTATION_ROADMAP.md` → Phase 3 section

**Quick Integration:**
```cpp
// In your analysis panel or wherever you want anomaly detection:
#include "AnomalyDetector.hpp"

auto detector = std::make_unique<ai::AnomalyDetector>(aiService);
auto anomalies = detector->Detect(eventsContainer, 1000);

for (const auto& anomaly : anomalies) {
    std::cout << anomaly.GetTypeString() << ": " 
              << anomaly.explanation << "\n";
}
```

**Time:** ~10 hours (6 hours code + 4 hours UI integration + testing)

---

## Issue #4: Build Filter Assistant

**Why:** Users don't know regex; need "ask AI" to filter

**Basic Structure:**
```cpp
// src/application/ai/FilterAssistant.hpp
namespace ai {

class FilterAssistant {
public:
    explicit FilterAssistant(std::shared_ptr<IAIService> aiService);
    
    /// User says "Show database errors", AI returns filter suggestions
    std::vector<FilterSuggestion> SuggestFilters(
        const std::string& userIntent,
        const mvc::IModel& events);
    
    bool IsReady() const;
};

}  // namespace ai
```

**UI Dialog Flow:**
```
1. User clicks "Ask AI to filter"
2. Dialog: "What do you want to find?" [text input]
3. User: "Database errors after 3pm"
4. AI suggests 3 filters with previews
5. User clicks one, filter applies
```

**For full implementation, see:** `IMPLEMENTATION_ROADMAP.md` → Phase 4 section

**Time:** ~7 hours

---

## Compilation Cheat Sheet

```bash
# Navigate to project
cd /Users/Marcin/workspace/cpp/LogViewer

# Build everything (debug, with tests)
cmake --build build/macos-debug-qt -j4

# Run tests
ctest --preset macos-debug-test --verbose

# Build specific target
cmake --build build/macos-debug-qt --target LogViewer -j4

# Clean rebuild
rm -rf build/macos-debug-qt && cmake -B build/macos-debug-qt -S . --preset macos-debug-qt && cmake --build build/macos-debug-qt -j4

# Run with sanitizers
cmake --build build/macos-debug-qt --target LogViewer -j4
./build/macos-debug-qt/bin/LogViewer

# Check clang-tidy
cmake --build build/macos-debug-qt --target tidy
```

---

## Testing Your Changes

### Add Unit Test
```cpp
// tests/YourNewFeatureTest.cpp
#include <gtest/gtest.h>
#include "YourNewFeature.hpp"

namespace {

TEST(YourNewFeatureTest, BasicFunctionality) {
    // Arrange
    auto feature = YourNewFeature();
    
    // Act
    auto result = feature.DoSomething();
    
    // Assert
    EXPECT_TRUE(result);
}

}  // namespace
```

### Run Specific Test
```bash
ctest --preset macos-debug-test -R "YourNewFeature" --verbose
```

---

## Common Patterns in LogViewer

### Return Result<T, E> Instead of Exceptions
```cpp
// GOOD:
util::Result<std::string, error::Error> MyFunction() {
    if (somethingBad) {
        return error::Error("Detailed error message");
    }
    return std::string("success value");
}

// Usage:
auto result = MyFunction();
if (result.isOk()) {
    std::cout << result.unwrap() << "\n";
} else {
    std::cout << "Error: " << result.unwrapErr().message() << "\n";
}
```

### Logging (Structured)
```cpp
#include "Logger.hpp"

util::Logger::Info("Starting analysis: {}", eventCount);
util::Logger::Debug("Detailed info: x={}, y={}", x, y);
util::Logger::Warn("Warning: {}", reason);
util::Logger::Error("Error: {}", problem);
```

### Thread Safety in EventsContainer
```cpp
{
    std::shared_lock<std::shared_mutex> lock(m_mutex);  // Read
    const auto& event = GetEvent(index);
}

{
    std::unique_lock<std::shared_mutex> lock(m_mutex);  // Write
    AddEvent(std::move(event));
}
```

---

## Common Build Issues & Fixes

### Issue: "cannot find llama.h"
```bash
# Make sure llama.cpp is in thirdparty/
ls thirdparty/llama.cpp/

# If missing, install it:
cd thirdparty
git clone https://github.com/ggerganov/llama.cpp.git
```

### Issue: "undefined reference to Qt symbols"
```bash
# Qt not found - check CMakeLists.txt
grep -r "Qt6::" CMakeLists.txt

# Make sure Qt 6.5+ is installed:
brew install qt6@6.5  # macOS
```

### Issue: "compilation errors in third-party"
```bash
# Clean and rebuild thirdparty
rm -rf build/
cmake -B build/macos-debug-qt --preset macos-debug-qt
cmake --build build/macos-debug-qt -j4
```

---

## Where to Find Things

| What | Where |
|------|-------|
| Main entry point | `src/main/MyAppQt.cpp` |
| MVC controller | `src/application/mvc/MainController.hpp` |
| Data model | `src/application/db/EventsContainer.hpp` |
| Log parsing | `src/application/parsers/` |
| UI components | `src/application/ui/qt/` |
| Filters | `src/application/filters/Filter.hpp` |
| AI services | `plugins/ai/` |
| Tests | `tests/` |
| Config | `src/application/config/Config.hpp` |
| Plugins | `src/application/plugins/PluginManager.hpp` |

---

## Making a Commit

```bash
git status                          # Check what changed
git diff src/file.cpp              # Review changes
git add src/file.cpp               # Stage for commit
git commit -m "feat: Implement anomaly detection

- Add AnomalyDetector class for error spike detection
- Support 6 anomaly types
- AI-powered analysis with Claude API
- Fixes #123"
```

**Good Commit Messages:**
- Start with: `feat:`, `fix:`, `refactor:`, `docs:`, `test:`
- First line: 50 chars max, summary
- Blank line, then detailed description
- Reference issues: `Fixes #123`

---

## Performance Tips

### Profile with Instruments
```bash
# macOS only:
xcrun xctrace record --template "System Trace" \
  ./build/macos-debug-qt/bin/LogViewer
```

### Check Thread Safety
```bash
# Rebuild with ThreadSanitizer
rm -rf build/
cmake -B build/macos-debug-qt --preset macos-debug-qt \
  -DLOGVIEWER_SANITIZER=Thread
cmake --build build/macos-debug-qt -j4

# Run tests - will report thread safety issues
ctest --preset macos-debug-test
```

---

## Resources

- **Code:** See `ARCHITECTURE_ANALYSIS.md` for full analysis
- **Implementation:** See `IMPLEMENTATION_ROADMAP.md` for code examples
- **Planning:** See `ANALYSIS_SUMMARY.md` for priorities

---

## Questions?

Check these first:
1. `src/application/CMakeLists.txt` - build structure
2. `.clang-format` - code style rules
3. `docs/ARCHITECTURE.md` - system design
4. `docs/SDK_GETTING_STARTED.md` - plugin development

---

**Last Updated:** 2026-06-15  
**Ready to Implement?** Start with Issue #1 (30 min), then #2 (6 hours)
