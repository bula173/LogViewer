# LogViewer: Implementation Roadmap & Code Examples

**Timeline:** Organized by priority and effort  
**Last Updated:** 2026-06-15

---

## PHASE 1: Quick Fixes (2-3 days)
### Target: Resolve architectural TODOs and code quality issues

---

### 1.1 Generalize Service Interface (30 min)
**Goal:** Remove TODO from MainWindow.hpp, enable non-AI services

**Current State:**
```cpp
// src/application/ui/qt/MainWindow.hpp:50
std::shared_ptr<ai::IAIService> m_pluginService;  // TODO: Generalize beyond AI
```

**Step 1: Create generic service interface**
```cpp
// src/application/services/IService.hpp (NEW FILE)
#pragma once
#include <string>
#include <memory>

namespace services {

/// Base interface for all pluggable services
class IService {
public:
    virtual ~IService() = default;
    
    /// Unique identifier for this service instance
    virtual std::string GetServiceId() const = 0;
    
    /// Type of service ("ai", "analyzer", "exporter", etc.)
    virtual std::string GetServiceType() const = 0;
};

}  // namespace services
```

**Step 2: Make IAIService inherit from IService**
```cpp
// src/application/ai/IAIService.hpp (MODIFY)
#pragma once
#include "IService.hpp"
#include <functional>

namespace ai {

class IAIService : public services::IService {
public:
    virtual ~IAIService() = default;
    
    // IService implementation
    std::string GetServiceType() const override { return "ai"; }
    
    // AI-specific interface
    virtual std::string SendPrompt(const std::string& prompt,
        std::function<void(const std::string&)> callback = nullptr) = 0;
    
    virtual bool IsAvailable() const = 0;
    virtual std::string GetModelName() const = 0;
    virtual void SetModelName(const std::string& modelName) = 0;
    virtual std::string GetProviderName() const = 0;
};

}  // namespace ai
```

**Step 3: Update MainWindow**
```cpp
// src/application/ui/qt/MainWindow.hpp (MODIFY)
#include "IService.hpp"

// OLD:
// std::shared_ptr<ai::IAIService> m_pluginService;  // TODO: Generalize

// NEW:
std::shared_ptr<services::IService> m_currentService;
std::shared_ptr<ai::IAIService> GetAIService() const {
    return std::dynamic_pointer_cast<ai::IAIService>(m_currentService);
}
```

**Compile & Test:**
```bash
cd /Users/Marcin/workspace/cpp/LogViewer
cmake --build build/macos-debug-qt --target LogViewer_tests
ctest --preset macos-debug-test
```

**Verification:**
- [ ] All includes resolve
- [ ] No TODO comment remains
- [ ] Tests pass (MainWindow tests if any)
- [ ] Code compiles with -Wextra

---

### 1.2 Add Error Context to GemmaInferenceEngine (2 hours)
**Goal:** Replace bool returns with Result<>, add detailed error info

**Step 1: Identify return points**
```bash
grep -n "return false\|return true" \
  src/application/ai/GemmaInferenceEngine.cpp | head -20
```

**Step 2: Replace with Result<T, error::Error>**

Current:
```cpp
// src/application/ai/GemmaInferenceEngine.cpp:38-46
bool GemmaInferenceEngine::Initialize() {
    if (!s_impl) s_impl = std::make_unique<Impl>();
    if (s_impl->initialized) return s_impl->available;
    s_impl->initialized = true;
    // ...
    return false;  // No context!
}
```

New:
```cpp
// src/application/ai/GemmaInferenceEngine.hpp (MODIFY)
namespace ai {

struct GemmaResult {
    bool success {false};
    std::string error;
    std::string detail;  // Extra context
};

class GemmaInferenceEngine {
public:
    static GemmaResult Initialize();  // Changed from bool
    static GemmaActorResult ExtractActors(const std::set<std::string>& msgs);
    // ... rest unchanged
};

}  // namespace ai
```

Implementation:
```cpp
// src/application/ai/GemmaInferenceEngine.cpp (MODIFY)
GemmaResult GemmaInferenceEngine::Initialize() {
    if (!s_impl) s_impl = std::make_unique<Impl>();
    if (s_impl->initialized) {
        return {s_impl->available, "", ""};
    }
    s_impl->initialized = true;
    
    // Determine model path...
    if (s_modelPath.empty()) {
        const auto& configPath = config::GetConfig().GetConfigFilePath();
        const auto appDir = std::filesystem::path(configPath).parent_path();
        s_modelPath = (appDir / "models" / "gemma-2b.gguf").string();
    }
    
    util::Logger::Info("[Gemma] Model path: {}", s_modelPath);
    
    // Check model existence
    if (!std::filesystem::exists(s_modelPath)) {
        std::string detail = fmt::format(
            "Model not found at {}. Download via Tools > Download AI Model",
            s_modelPath);
        s_impl->available = false;
        return {false, "Model not found", detail};
    }
    
    try {
        llama_backend_init();
        llama_model_params params = llama_model_default_params();
        params.n_gpu_layers = -1;
        
        s_impl->model = llama_load_model_from_file(s_modelPath.c_str(), params);
        if (!s_impl->model) {
            // Try to diagnose why
            std::string detail;
            if (std::filesystem::file_size(s_modelPath) < 1000000000) {
                detail = "Model file too small (<1GB). May be corrupted.";
            } else {
                detail = "llama.cpp failed to load model. Check compatibility.";
            }
            s_impl->available = false;
            return {false, "Failed to load model", detail};
        }
        
        // Create context
        llama_context_params ctx_params = llama_context_default_params();
        ctx_params.n_ctx = 2048;
        ctx_params.n_threads = 4;
        
        s_impl->ctx = llama_new_context_with_model(s_impl->model, ctx_params);
        if (!s_impl->ctx) {
            llama_free_model(s_impl->model);
            s_impl->model = nullptr;
            s_impl->available = false;
            return {false, "Failed to create context",
                    "Insufficient memory or GPU resources"};
        }
        
        util::Logger::Info("[Gemma] Initialization succeeded");
        s_impl->available = true;
        return {true, "", ""};
    }
    catch (const std::exception& e) {
        s_impl->available = false;
        return {false, "Initialization exception", std::string(e.what())};
    }
}
```

**Update callers:**
```cpp
// src/application/analyzers/ActorDiscoverer.cpp (MODIFY)
ActorDiscoveryResult ActorDiscoverer::DiscoverWithAI(...) {
    auto result = ai::GemmaInferenceEngine::Initialize();
    if (!result.success) {
        util::Logger::Debug("[ActorDiscoverer] Gemma unavailable: {}",
                           result.error);
        // Fall back to heuristic
        return Discover(events, sampleLimit);
    }
    // Use AI...
}
```

---

### 1.3 Fix GemmaDirectionResult Duplication (30 min)
**Goal:** Remove redundant confidence field

**Current:**
```cpp
// src/application/ai/GemmaInferenceEngine.hpp:31-37
struct DirectionPattern {
    std::string senderField;
    std::string receiverField;
    std::set<std::string> senderKeywords;
    std::set<std::string> receiverKeywords;
    std::set<std::string> directionKeywords;
    int confidence {0};             // ← In pattern
    std::string description;
};

struct GemmaDirectionResult {
    DirectionPattern pattern;
    int confidence {0};             // ← Duplicated here!
    std::string error;
};
```

**Fix:**
```cpp
// src/application/ai/GemmaInferenceEngine.hpp (MODIFY)
struct GemmaDirectionResult {
    std::optional<DirectionPattern> pattern;  // nullopt = failed
    std::string error;  // Empty = success
    
    bool isSuccess() const {
        return pattern.has_value();
    }
};
```

**Update implementation:**
```cpp
// src/application/ai/GemmaInferenceEngine.cpp (MODIFY)
GemmaDirectionResult GemmaInferenceEngine::DetectDirections(
    const std::set<std::string>& sampleMessages)
{
    if (!IsAvailable()) {
        return {std::nullopt, "Gemma model not available"};
    }
    
    // Heuristic analysis...
    DirectionPattern pattern;
    pattern.confidence = 65;  // Example
    
    return {pattern, ""};  // Success
}
```

**Update callers:**
```cpp
// Example usage:
auto result = GemmaInferenceEngine::DetectDirections(messages);
if (result.pattern) {
    std::cout << "Confidence: " << result.pattern->confidence << "\n";
} else {
    std::cout << "Error: " << result.error << "\n";
}
```

---

### 1.4 Add Input Validation to LogAnalyzer (1 hour)
**Goal:** Validate preconditions, return Result<>

**Current:**
```cpp
// plugins/ai/LogAnalyzer.hpp:37-39
std::string Analyze(AnalysisType type, size_t maxEvents = 100,
                   const std::vector<unsigned long>* filteredIndices = nullptr);
```

**Enhanced:**
```cpp
// plugins/ai/LogAnalyzer.hpp (MODIFY)
#pragma once
#include "Result.hpp"
#include "Error.hpp"
#include <string>

namespace ai {

class LogAnalyzer {
public:
    explicit LogAnalyzer(std::shared_ptr<AIServiceWrapper> aiService);
    
    /**
     * @brief Perform analysis on current log data
     * @param type Type of analysis
     * @param maxEvents Max events to include (0 = all)
     * @param filteredIndices Optional specific event indices
     * @return Result with analysis text or error
     */
    util::Result<std::string, error::Error> Analyze(
        AnalysisType type,
        size_t maxEvents = 100,
        const std::vector<unsigned long>* filteredIndices = nullptr);
    
    bool IsReady() const;
    
private:
    std::shared_ptr<AIServiceWrapper> m_aiService;
    
    util::Result<std::string, error::Error> ValidateInputs(
        size_t maxEvents,
        const std::vector<unsigned long>* filteredIndices);
};

}  // namespace ai
```

**Implementation:**
```cpp
// plugins/ai/LogAnalyzer.cpp (MODIFY)

namespace ai {

util::Result<std::string, error::Error> LogAnalyzer::Analyze(
    AnalysisType type,
    size_t maxEvents,
    const std::vector<unsigned long>* filteredIndices)
{
    // Validate AI service
    if (!m_aiService || !m_aiService->IsAvailable()) {
        return util::Result<std::string, error::Error>::err(
            error::Error("AI service not available"));
    }
    
    // Validate indices if provided
    if (filteredIndices && filteredIndices->empty()) {
        return util::Result<std::string, error::Error>::err(
            error::Error("No events selected for analysis"));
    }
    
    // Validate maxEvents
    if (maxEvents == 0 && !filteredIndices) {
        // Warning: analyzing all events could be expensive
        util::Logger::Warn("[LogAnalyzer] Analyzing all events (0 limit)");
    }
    
    try {
        std::string formattedData = FormatEventsForAI(maxEvents, filteredIndices);
        if (formattedData.empty()) {
            return util::Result<std::string, error::Error>::err(
                error::Error("No log data to analyze"));
        }
        
        std::string prompt = BuildPrompt(type, formattedData);
        std::string result = m_aiService->SendPrompt(prompt);
        
        return util::Result<std::string, error::Error>::ok(result);
    }
    catch (const std::exception& e) {
        return util::Result<std::string, error::Error>::err(
            error::Error(fmt::format("Analysis failed: {}", e.what())));
    }
}

}  // namespace ai
```

**Update UI callers:**
```cpp
// In analysis panel or wherever Analyze() is called:
auto result = analyzer.Analyze(
    LogAnalyzer::AnalysisType::ErrorAnalysis,
    100,
    nullptr);

if (result.isOk()) {
    displayAnalysisResults(result.unwrap());
} else {
    showErrorDialog(result.unwrapErr().message());
}
```

---

## PHASE 2: Actual Gemma Inference (6-8 hours)
### Target: Make embedded model actually useful

---

### 2.1 Implement LLM Inference for Actor Extraction

**Goal:** Use Gemma 2B to extract actors from sample messages instead of heuristics

**Create inference utility:**
```cpp
// src/application/ai/GemmaInference.hpp (NEW FILE)
#pragma once
#include <string>
#include <set>
#include <vector>
#include <nlohmann/json.hpp>

namespace ai {

/// Helper for tokenization and inference with llama.cpp
class GemmaInference {
public:
    static std::string InferActorExtraction(
        llama_context* ctx,
        const std::set<std::string>& sampleMessages);
    
    static std::string InferDirectionDetection(
        llama_context* ctx,
        const std::set<std::string>& sampleMessages);
    
private:
    /// Build system prompt for actor extraction
    static std::string BuildActorExtractionPrompt(
        const std::set<std::string>& messages);
    
    /// Run inference with given prompt
    static std::string RunInference(
        llama_context* ctx,
        const std::string& prompt,
        int maxTokens = 512);
    
    /// Parse JSON response from LLM
    static std::set<std::string> ParseActorsFromResponse(
        const std::string& response);
};

}  // namespace ai
```

**Implementation:**
```cpp
// src/application/ai/GemmaInference.cpp (NEW FILE)
#include "GemmaInference.hpp"
#include "Logger.hpp"
#include <llama.h>
#include <sstream>
#include <regex>

namespace ai {

std::string GemmaInference::InferActorExtraction(
    llama_context* ctx,
    const std::set<std::string>& sampleMessages)
{
    if (!ctx) return "";
    
    std::string prompt = BuildActorExtractionPrompt(sampleMessages);
    return RunInference(ctx, prompt, 512);
}

std::string GemmaInference::BuildActorExtractionPrompt(
    const std::set<std::string>& messages)
{
    std::ostringstream oss;
    oss << R"(Extract all service, component, or actor names from these log messages.
Return a JSON object with a single key "actors" containing an array of strings.

Examples of actors: "payment-service", "user-id-123", "database", "cache", "api-gateway"

Messages:
)";
    
    for (const auto& msg : messages) {
        oss << "- " << msg << "\n";
    }
    
    oss << R"(
Return ONLY valid JSON, no other text:
{"actors": ["actor1", "actor2", ...]}
)";
    
    return oss.str();
}

std::string GemmaInference::RunInference(
    llama_context* ctx,
    const std::string& prompt,
    int maxTokens)
{
    // Tokenize
    auto tokens = llama_tokenize(llama_get_model(ctx),
        prompt.c_str(), prompt.length(), false);
    
    if (tokens.empty()) {
        util::Logger::Warn("[Gemma] Tokenization failed");
        return "";
    }
    
    util::Logger::Debug("[Gemma] Tokenized {} tokens", tokens.size());
    
    // Add tokens to context
    llama_batch batch = llama_batch_init(512, 0, 1);
    for (size_t i = 0; i < tokens.size(); ++i) {
        llama_batch_add(&batch, tokens[i], i, {0}, false);
    }
    batch.n_tokens = tokens.size();
    
    if (llama_decode(ctx, batch) != 0) {
        util::Logger::Error("[Gemma] Decode failed");
        llama_batch_free(batch);
        return "";
    }
    
    // Generate response
    std::string result;
    int n_generated = 0;
    auto n_ctx = llama_n_ctx(ctx);
    
    while (n_generated < maxTokens) {
        // Get next token
        llama_token new_token_id = llama_sampler_sample(/* sampler */, ctx, nullptr);
        n_generated++;
        
        // Decode token to string
        std::vector<char> buffer(32);
        int n = llama_token_to_piece(llama_get_model(ctx), new_token_id,
                                      buffer.data(), buffer.size(), true);
        if (n < 0) break;
        
        result.append(buffer.data(), n);
        
        // Stop at EOS
        if (llama_token_is_eog(llama_get_model(ctx), new_token_id)) {
            break;
        }
    }
    
    llama_batch_free(batch);
    return result;
}

std::set<std::string> GemmaInference::ParseActorsFromResponse(
    const std::string& response)
{
    std::set<std::string> actors;
    
    try {
        // Try to parse JSON
        size_t json_start = response.find('{');
        if (json_start == std::string::npos) return actors;
        
        size_t json_end = response.rfind('}');
        if (json_end == std::string::npos) return actors;
        
        std::string json_str = response.substr(json_start, json_end - json_start + 1);
        auto json = nlohmann::json::parse(json_str);
        
        if (json.contains("actors") && json["actors"].is_array()) {
            for (const auto& actor : json["actors"]) {
                if (actor.is_string()) {
                    actors.insert(actor.get<std::string>());
                }
            }
        }
    } catch (const std::exception& e) {
        util::Logger::Warn("[Gemma] JSON parse failed: {}", e.what());
    }
    
    return actors;
}

}  // namespace ai
```

**Update GemmaInferenceEngine to use it:**
```cpp
// src/application/ai/GemmaInferenceEngine.cpp (MODIFY)

GemmaActorResult GemmaInferenceEngine::ExtractActors(
    const std::set<std::string>& sampleMessages)
{
    GemmaActorResult result;
    
    if (!IsAvailable()) {
        result.error = "Gemma model not available";
        result.method = "heuristic";  // Falls back to heuristic
        return result;
    }
    
    try {
        // Use LLM inference
        std::string llmOutput = GemmaInference::InferActorExtraction(
            s_impl->ctx, sampleMessages);
        
        if (!llmOutput.empty()) {
            result.actors = GemmaInference::ParseActorsFromResponse(llmOutput);
            result.confidence = 85;  // High confidence from LLM
            result.method = "llm_inference";
            util::Logger::Info("[Gemma] LLM extracted {} actors",
                             result.actors.size());
            return result;
        }
    } catch (const std::exception& e) {
        util::Logger::Warn("[Gemma] Inference failed: {}, falling back to heuristic",
                          e.what());
    }
    
    // Fall back to heuristic
    result = PerformHeuristicActorExtraction(sampleMessages);
    result.method = "heuristic";
    return result;
}
```

---

## PHASE 3: AI-Powered Anomaly Detection (10-12 hours)
### High-value feature for users

---

### 3.1 Create AnomalyDetector Interface

```cpp
// src/application/ai/AnomalyDetector.hpp (NEW FILE)
#pragma once
#include "IAIService.hpp"
#include "IModel.hpp"
#include <string>
#include <vector>
#include <memory>

namespace ai {

enum class AnomalyType {
    ErrorSpike,
    LatencyAnomaly,
    UnusualActor,
    ProtocolViolation,
    ResourceExhaustion,
    SecurityAnomaly,
};

struct AnomalyReport {
    AnomalyType type;
    float severityScore {0.0f};  // 0.0-1.0
    std::vector<size_t> affectedEventIndices;
    std::string explanation;
    std::vector<std::string> recommendations;
    
    std::string GetTypeString() const;
};

class AnomalyDetector {
public:
    explicit AnomalyDetector(std::shared_ptr<IAIService> aiService);
    
    /// Find anomalies in event window
    std::vector<AnomalyReport> Detect(
        const mvc::IModel& events,
        size_t windowSize = 1000);
    
    /// Correlate multiple anomalies
    std::vector<std::vector<AnomalyReport>> FindCorrelated(
        const std::vector<AnomalyReport>& anomalies) const;
    
    /// Root cause analysis for specific anomaly
    std::string AnalyzeRootCause(
        const AnomalyReport& anomaly,
        const mvc::IModel& events);
    
    bool IsReady() const { return m_aiService && m_aiService->IsAvailable(); }
    
private:
    std::shared_ptr<IAIService> m_aiService;
    
    /// Build context about event window
    std::string FormatEventsForAnalysis(
        const mvc::IModel& events,
        const std::vector<size_t>& indices);
    
    /// Parse LLM response to extract anomalies
    std::vector<AnomalyReport> ParseAnomalies(
        const std::string& llmResponse,
        const mvc::IModel& events);
};

}  // namespace ai
```

**Implementation:**
```cpp
// src/application/ai/AnomalyDetector.cpp (NEW FILE)
#include "AnomalyDetector.hpp"
#include "Logger.hpp"
#include <nlohmann/json.hpp>
#include <sstream>
#include <cmath>

namespace ai {

std::string AnomalyReport::GetTypeString() const {
    switch (type) {
        case AnomalyType::ErrorSpike: return "Error Spike";
        case AnomalyType::LatencyAnomaly: return "Latency Anomaly";
        case AnomalyType::UnusualActor: return "Unusual Actor";
        case AnomalyType::ProtocolViolation: return "Protocol Violation";
        case AnomalyType::ResourceExhaustion: return "Resource Exhaustion";
        case AnomalyType::SecurityAnomaly: return "Security Anomaly";
        default: return "Unknown";
    }
}

AnomalyDetector::AnomalyDetector(std::shared_ptr<IAIService> aiService)
    : m_aiService(std::move(aiService))
{
}

std::vector<AnomalyReport> AnomalyDetector::Detect(
    const mvc::IModel& events,
    size_t windowSize)
{
    std::vector<AnomalyReport> results;
    
    if (!IsReady()) {
        util::Logger::Warn("[AnomalyDetector] AI service not ready");
        return results;
    }
    
    size_t start = 0;
    if (events.Size() > windowSize) {
        start = events.Size() - windowSize;
    }
    
    std::vector<size_t> indices;
    for (size_t i = start; i < events.Size(); ++i) {
        indices.push_back(i);
    }
    
    std::string eventsFormatted = FormatEventsForAnalysis(events, indices);
    
    std::string prompt = fmt::format(
        R"(Analyze these {} log events for anomalies.
        
Look for:
- Error rate spikes (sudden increase)
- Latency anomalies (unusually slow responses)
- Unusual actors/services
- Protocol violations (out of order, missing)
- Resource exhaustion signs
- Security anomalies (auth failures, etc)

Events:
{}

Return JSON:
{{
  "anomalies": [
    {{
      "type": "ErrorSpike|LatencyAnomaly|UnusualActor|ProtocolViolation|ResourceExhaustion|SecurityAnomaly",
      "severity": 0.75,
      "affected_event_indices": [1, 5, 23],
      "explanation": "...",
      "recommendations": ["...", "..."]
    }}
  ]
}})",
        indices.size(),
        eventsFormatted);
    
    std::string llmResponse = m_aiService->SendPrompt(prompt);
    results = ParseAnomalies(llmResponse, events);
    
    return results;
}

std::vector<AnomalyReport> AnomalyDetector::ParseAnomalies(
    const std::string& llmResponse,
    const mvc::IModel& events)
{
    std::vector<AnomalyReport> results;
    
    try {
        // Extract JSON from response
        size_t json_start = llmResponse.find('{');
        if (json_start == std::string::npos) return results;
        
        std::string json_str = llmResponse.substr(json_start);
        auto json = nlohmann::json::parse(json_str);
        
        if (!json.contains("anomalies")) return results;
        
        for (const auto& anomaly : json["anomalies"]) {
            AnomalyReport report;
            
            // Parse type
            std::string typeStr = anomaly.value("type", "");
            if (typeStr == "ErrorSpike") report.type = AnomalyType::ErrorSpike;
            else if (typeStr == "LatencyAnomaly") report.type = AnomalyType::LatencyAnomaly;
            else if (typeStr == "UnusualActor") report.type = AnomalyType::UnusualActor;
            else if (typeStr == "ProtocolViolation") report.type = AnomalyType::ProtocolViolation;
            else if (typeStr == "ResourceExhaustion") report.type = AnomalyType::ResourceExhaustion;
            else if (typeStr == "SecurityAnomaly") report.type = AnomalyType::SecurityAnomaly;
            else continue;
            
            report.severityScore = anomaly.value("severity", 0.5f);
            report.explanation = anomaly.value("explanation", "");
            
            // Parse affected indices
            if (anomaly.contains("affected_event_indices")) {
                for (const auto& idx : anomaly["affected_event_indices"]) {
                    if (idx.is_number_integer()) {
                        report.affectedEventIndices.push_back(idx.get<size_t>());
                    }
                }
            }
            
            // Parse recommendations
            if (anomaly.contains("recommendations")) {
                for (const auto& rec : anomaly["recommendations"]) {
                    if (rec.is_string()) {
                        report.recommendations.push_back(rec.get<std::string>());
                    }
                }
            }
            
            results.push_back(report);
        }
    } catch (const std::exception& e) {
        util::Logger::Warn("[AnomalyDetector] JSON parse failed: {}", e.what());
    }
    
    return results;
}

std::string AnomalyDetector::FormatEventsForAnalysis(
    const mvc::IModel& events,
    const std::vector<size_t>& indices)
{
    std::ostringstream oss;
    
    for (size_t idx : indices) {
        const auto& event = events.GetItem(idx);
        oss << fmt::format("[{}] ", idx);
        
        for (const auto& [key, value] : event.GetItems()) {
            oss << fmt::format("{}={} ", key, value);
        }
        oss << "\n";
    }
    
    return oss.str();
}

}  // namespace ai
```

---

## PHASE 4: Filter Assistant (7-8 hours)
### Natural language to filters

---

### 4.1 Create FilterAssistant

```cpp
// src/application/ai/FilterAssistant.hpp (NEW FILE)
#pragma once
#include "IAIService.hpp"
#include "Filter.hpp"
#include "IModel.hpp"
#include <vector>
#include <memory>

namespace ai {

struct FilterSuggestion {
    filters::FilterPtr suggestedFilter;
    float confidenceScore {0.0f};
    std::string explanation;
    std::vector<unsigned long> matchedIndices;  // Preview
};

class FilterAssistant {
public:
    explicit FilterAssistant(std::shared_ptr<IAIService> aiService);
    
    /// Suggest filters from natural language intent
    std::vector<FilterSuggestion> SuggestFilters(
        const std::string& userIntent,
        const mvc::IModel& events,
        size_t previewLimit = 20);
    
    /// Generate filter from selection
    FilterSuggestion GenerateFromSelection(
        const std::vector<size_t>& selectedIndices,
        const mvc::IModel& events);
    
    /// Refine filter based on feedback
    FilterSuggestion Refine(
        const filters::FilterPtr& currentFilter,
        const std::string& feedback,
        const mvc::IModel& events);
    
    bool IsReady() const { return m_aiService && m_aiService->IsAvailable(); }
    
private:
    std::shared_ptr<IAIService> m_aiService;
    
    /// Build schema of available fields for context
    std::string GetSchemaContext(const mvc::IModel& events);
    
    /// Parse AI response to Filter objects
    std::vector<FilterSuggestion> ParseFilterSuggestions(
        const std::string& llmResponse,
        const mvc::IModel& events,
        size_t previewLimit);
};

}  // namespace ai
```

---

## SUMMARY: Implementation Checklist

### Phase 1: Quick Fixes (2-3 days)
- [ ] 1.1 Generalize Service Interface (30 min)
- [ ] 1.2 Add Error Context to GemmaInferenceEngine (2 hrs)
- [ ] 1.3 Fix GemmaDirectionResult Duplication (30 min)
- [ ] 1.4 Add Input Validation to LogAnalyzer (1 hr)
- [ ] Total Phase 1: 4 hours, high confidence

### Phase 2: Actual Gemma Inference (6-8 hours)
- [ ] 2.1 Create GemmaInference utility (4 hrs)
- [ ] 2.2 Implement ExtractActors with LLM (2 hrs)
- [ ] 2.3 Test and integrate (1 hr)
- [ ] Total Phase 2: 7 hours, medium complexity

### Phase 3: Anomaly Detection (10-12 hours)
- [ ] 3.1 Create AnomalyDetector interface & impl (6 hrs)
- [ ] 3.2 Create UI panel for anomalies (4 hrs)
- [ ] 3.3 Test and integrate (1 hr)
- [ ] Total Phase 3: 11 hours, high value

### Phase 4: Filter Assistant (7-8 hours)
- [ ] 4.1 Create FilterAssistant (5 hrs)
- [ ] 4.2 Create UI dialog (2 hrs)
- [ ] 4.3 Test and integrate (1 hr)
- [ ] Total Phase 4: 8 hours, high value

**Total Recommended Timeline:** 30 hours over 2-3 sprints
**Order:** Phase 1 → Phase 2 → Phase 3 & 4 in parallel

---

**Status:** Ready for implementation  
**Owner:** Marcin Kwiatkowski  
**Last Updated:** 2026-06-15
