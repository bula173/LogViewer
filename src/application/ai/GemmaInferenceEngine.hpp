#pragma once

#include <memory>
#include <optional>
#include <set>
#include <string>

namespace ai
{

/// Result from Gemma inference for actor extraction
struct GemmaActorResult
{
    std::set<std::string> actors;
    int confidence {0};  // 0-100
    std::string error;   // Empty if success
};

/// Direction pattern detected in logs
struct DirectionPattern
{
    std::string senderField;        ///< Field that identifies sender
    std::string receiverField;      ///< Field that identifies receiver
    std::set<std::string> senderKeywords;      ///< Keywords indicating sender
    std::set<std::string> receiverKeywords;    ///< Keywords indicating receiver
    std::set<std::string> directionKeywords;   ///< Keywords showing direction
    int confidence {0};             ///< 0-100
    std::string description;        ///< Human-readable summary
};

/// Result from direction discovery
struct GemmaDirectionResult
{
    std::optional<DirectionPattern> pattern;  // nullopt if failed
    std::string error;  // Empty if success

    bool isSuccess() const {
        return pattern.has_value();
    }
};

/// Gemma 2B inference engine for local actor discovery.
/// Manages model loading, downloading, and inference via llama.cpp.
class GemmaInferenceEngine
{
public:
    /// Initialize the engine. Tries to find/load model, returns false if unavailable.
    static bool Initialize();

    /// Check if Gemma is available (model found or downloadable)
    static bool IsAvailable();

    /// Extract actors from log messages using Gemma 2B.
    /// Returns empty result if engine unavailable or inference fails.
    static GemmaActorResult ExtractActors(const std::set<std::string>& sampleMessages);

    /// Detect message direction patterns (who sends to whom) using AI analysis.
    /// Analyzes log messages to identify sender/receiver fields and keywords.
    /// Returns direction pattern with confidence score.
    static GemmaDirectionResult DetectDirections(const std::set<std::string>& sampleMessages);

    /// Get/set custom model path (default: ~/.logviewer/models/gemma-2b.gguf)
    static std::string GetModelPath();
    static void SetModelPath(const std::string& path);

    /// Shutdown the engine (frees resources)
    static void Shutdown();

    /// Download model from URL to local path
    /// Returns error string (empty if success)
    /// Shows progress via logger
    static std::string DownloadModel(const std::string& url, const std::string& filename);

    /// Check if model is already downloaded
    static bool HasModel();

    /// Get model download URL
    static std::string GetModelDownloadUrl();

private:
    struct Impl;
    static std::unique_ptr<Impl> s_impl;
    static std::string s_modelPath;

    GemmaInferenceEngine() = delete;  // Static only
};

} // namespace ai
