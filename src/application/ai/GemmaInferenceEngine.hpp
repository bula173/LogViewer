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

    /// Get/set custom model path (default: ~/.logviewer/models/gemma-2b.gguf)
    static std::string GetModelPath();
    static void SetModelPath(const std::string& path);

    /// Shutdown the engine (frees resources)
    static void Shutdown();

private:
    struct Impl;
    static std::unique_ptr<Impl> s_impl;
    static std::string s_modelPath;

    GemmaInferenceEngine() = delete;  // Static only
};

} // namespace ai
