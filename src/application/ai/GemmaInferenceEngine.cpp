#include "GemmaInferenceEngine.hpp"

#include "Logger.hpp"
#include "Config.hpp"

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <sstream>

namespace ai
{

// Implementation details (opaque pointer pattern)
struct GemmaInferenceEngine::Impl
{
    bool initialized {false};
    bool available {false};
    // void* llama_model = nullptr;  // llama_model pointer (when llama.cpp integrated)
    // void* llama_ctx = nullptr;    // llama_context pointer
};

std::unique_ptr<GemmaInferenceEngine::Impl> GemmaInferenceEngine::s_impl;
std::string GemmaInferenceEngine::s_modelPath;

bool GemmaInferenceEngine::Initialize()
{
    if (!s_impl)
        s_impl = std::make_unique<Impl>();

    if (s_impl->initialized)
        return s_impl->available;

    s_impl->initialized = true;

    // Step 1: Determine model path
    if (s_modelPath.empty())
    {
        // Default: ~/.logviewer/models/gemma-2b.gguf
        const auto& configPath = config::GetConfig().GetConfigFilePath();
        const auto appDir = std::filesystem::path(configPath).parent_path();
        s_modelPath = (appDir / "models" / "gemma-2b.gguf").string();
    }

    util::Logger::Info("[Gemma] Model path: {}", s_modelPath);

    // Step 2: Check if model exists
    if (std::filesystem::exists(s_modelPath))
    {
        util::Logger::Info("[Gemma] Found model at {}", s_modelPath);
        s_impl->available = true;
        // TODO: Load model via llama_load_model_from_file when llama.cpp integrated
        return true;
    }

    // Step 3: Try to download if missing
    util::Logger::Info("[Gemma] Model not found. Would download from HuggingFace...");
    util::Logger::Info("[Gemma] URL: https://huggingface.co/google/gemma-2b-it-gguf");
    util::Logger::Info("[Gemma] Run: huggingface-cli download google/gemma-2b-it-gguf gemma-2b-it.gguf --local-dir ~/.logviewer/models");

    // TODO: Implement auto-download when network support added
    // For now, guide user to download manually

    util::Logger::Warn("[Gemma] Model not available. AI actor discovery disabled.");
    s_impl->available = false;
    return false;
}

bool GemmaInferenceEngine::IsAvailable()
{
    if (!s_impl)
        return false;
    if (!s_impl->initialized)
        Initialize();
    return s_impl->available;
}

GemmaActorResult GemmaInferenceEngine::ExtractActors(const std::set<std::string>& sampleMessages)
{
    GemmaActorResult result;

    if (!IsAvailable())
    {
        result.error = "Gemma model not available";
        return result;
    }

    try
    {
        // Build prompt for Gemma 2B
        std::string prompt = "Analyze these log lines and extract all actor/component names.\n\n";
        prompt += "Log lines:\n";

        int lineNum = 1;
        for (const auto& msg : sampleMessages)
        {
            if (lineNum > 10) break;  // Limit to 10 examples
            prompt += std::to_string(lineNum) + ". " + msg + "\n";
            ++lineNum;
        }

        prompt += "\nReturn a JSON object with:\n";
        prompt += "{\n";
        prompt += "  \"actors\": [list of actor names],\n";
        prompt += "  \"confidence\": 0-100\n";
        prompt += "}\n";

        util::Logger::Debug("[Gemma] Inference prompt:\n{}", prompt);

        // TODO: Run inference via llama.cpp
        // std::string response = llama_inference(s_impl->llama_ctx, prompt);
        // Parse JSON response and populate result

        // Placeholder for demonstration
        result.error = "Gemma inference not yet integrated (llama.cpp pending)";
        result.confidence = 0;

        return result;
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[Gemma] Inference failed: {}", e.what());
        result.error = std::string("Gemma inference error: ") + e.what();
        return result;
    }
}

std::string GemmaInferenceEngine::GetModelPath()
{
    return s_modelPath;
}

void GemmaInferenceEngine::SetModelPath(const std::string& path)
{
    s_modelPath = path;
    if (s_impl)
        s_impl->initialized = false;  // Force re-initialization
    util::Logger::Info("[Gemma] Model path set to: {}", path);
}

void GemmaInferenceEngine::Shutdown()
{
    if (!s_impl)
        return;

    // TODO: Cleanup llama.cpp resources
    // if (s_impl->llama_ctx) llama_free(s_impl->llama_ctx);
    // if (s_impl->llama_model) llama_free_model(s_impl->llama_model);

    s_impl.reset();
    util::Logger::Info("[Gemma] Engine shutdown");
}

} // namespace ai
