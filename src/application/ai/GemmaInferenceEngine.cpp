#include "GemmaInferenceEngine.hpp"

#include "Logger.hpp"
#include "Config.hpp"

#include <llama.h>
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <cmath>
#include <cstdlib>
#include <vector>

namespace ai
{

// Configuration constants for Gemma model
constexpr const char* GEMMA_MODEL_NAME = "gemma-2b.gguf";
constexpr const char* GEMMA_MODELS_DIR = "models";

// Implementation details (opaque pointer pattern)
struct GemmaInferenceEngine::Impl
{
    bool initialized {false};
    bool available {false};
    llama_model* model {nullptr};
    llama_context* ctx {nullptr};

    ~Impl()
    {
        if (ctx) llama_free(ctx);
        if (model) llama_free_model(model);
    }
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
        s_modelPath = (appDir / GEMMA_MODELS_DIR / GEMMA_MODEL_NAME).string();
    }

    util::Logger::Info("[Gemma] Model path: {}", s_modelPath);

    // Step 2: Check if model exists and load
    if (std::filesystem::exists(s_modelPath))
    {
        util::Logger::Info("[Gemma] Found model at {}", s_modelPath);

        try
        {
            // Initialize llama.cpp backend
            llama_backend_init();
            util::Logger::Info("[Gemma] llama.cpp backend initialized");

            // Load model with default parameters
            llama_model_params model_params = llama_model_default_params();
            model_params.n_gpu_layers = -1;  // Offload to GPU if available

            s_impl->model = llama_load_model_from_file(s_modelPath.c_str(), model_params);

            if (!s_impl->model)
            {
                std::string detail;
                try {
                    auto fileSize = std::filesystem::file_size(s_modelPath);
                    if (fileSize < 100000000) {  // Less than 100MB
                        detail = " (file too small: " + std::to_string(fileSize / 1000000) +
                                " MB, expected ~1.5GB)";
                    } else {
                        detail = " (possible file corruption or llama.cpp incompatibility)";
                    }
                } catch (...) {
                    detail = " (cannot stat file)";
                }
                util::Logger::Error("[Gemma] Failed to load model from {}{}",
                                   s_modelPath, detail);
                s_impl->available = false;
                return false;
            }

            util::Logger::Info("[Gemma] Model loaded: {}", s_modelPath);

            // Create context
            llama_context_params ctx_params = llama_context_default_params();
            ctx_params.n_ctx = 2048;      // Context window size
            ctx_params.n_threads = 4;     // CPU threads
            ctx_params.type_k = GGML_TYPE_Q8_0;  // Key/value quantization

            s_impl->ctx = llama_new_context_with_model(s_impl->model, ctx_params);

            if (!s_impl->ctx)
            {
                util::Logger::Error("[Gemma] Failed to create context (context_size={}). "
                                   "Possible causes: insufficient memory, GPU incompatibility, "
                                   "or system resource limits", ctx_params.n_ctx);
                llama_free_model(s_impl->model);
                s_impl->model = nullptr;
                s_impl->available = false;
                return false;
            }

            util::Logger::Info("[Gemma] Context created: {} tokens, {} threads",
                              llama_n_ctx(s_impl->ctx),
                              ctx_params.n_threads);

            s_impl->available = true;
            return true;
        }
        catch (const std::exception& e)
        {
            util::Logger::Error("[Gemma] Initialization failed: {}", e.what());
            s_impl->available = false;
            return false;
        }
    }

    // Model not found
    util::Logger::Warn("[Gemma] Model not found at {}. "
                      "To enable local AI inference, download the Gemma 2B model "
                      "via Tools > Download AI Model (1.5 GB)",
                      s_modelPath);

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

/// Static helper: Run inference on a prompt using the Gemma model
/// Returns pair<output_text, error_message> - if error is empty, output is valid
///
/// NOTE: This is a stub implementation. The actual inference loop requires:
/// - llama_tokenize(model, prompt, add_bos, special)
/// - llama_decode(ctx, batch) for context evaluation
/// - llama_sample_* functions for token selection
/// - llama_token_to_piece(model, token, buf, buf_size) for decoding
/// The exact API depends on the llama.cpp version used.
std::pair<std::string, std::string> GemmaInferenceEngine::RunInference(const std::string& prompt, int maxTokens)
{
    if (!s_impl || !s_impl->model || !s_impl->ctx)
        return {"", "Model not initialized"};

    try
    {
        // Step 1: Tokenize the prompt using raw C API
        std::vector<llama_token> tokens(prompt.size() + 10);  // Reserve space
        int32_t n_tokens = llama_tokenize(
            s_impl->model,
            prompt.c_str(),
            static_cast<int32_t>(prompt.length()),
            tokens.data(),
            static_cast<int32_t>(tokens.capacity()),
            true,  // add_bos
            false  // special
        );

        if (n_tokens <= 0)
            return {"", "Failed to tokenize prompt"};

        tokens.resize(n_tokens);

        util::Logger::Debug("[Gemma] Tokenized {} characters into {} tokens",
            prompt.length(), tokens.size());

        // Step 2: Ensure context has enough capacity for tokens + generation
        const int n_ctx = llama_n_ctx(s_impl->ctx);
        if (static_cast<int>(tokens.size()) >= n_ctx - 10)  // Leave headroom for generation
            return {"", "Prompt too long for context window"};

        // Step 3: Process prompt tokens through the model
        llama_batch batch = llama_batch_get_one(tokens.data(), static_cast<int32_t>(tokens.size()), 0, 0);
        if (llama_decode(s_impl->ctx, batch) != 0)
            return {"", "Error decoding prompt tokens"};

        // Step 4: Generate response tokens
        std::string result;
        std::vector<llama_token> generatedTokens;
        generatedTokens.reserve(maxTokens);

        llama_token endToken = llama_token_eos(s_impl->model);
        int tokensGenerated = 0;
        int n_vocab = llama_n_vocab(s_impl->model);

        for (int i = 0; i < maxTokens; ++i)
        {
            // Sample next token using greedy decoding (argmax of logits)
            float* logits = llama_get_logits(s_impl->ctx);
            if (!logits)
                break;

            // Find token with highest logit
            int32_t nextToken = 0;
            float maxLogit = logits[0];
            for (int32_t j = 1; j < n_vocab; ++j)
            {
                if (logits[j] > maxLogit)
                {
                    maxLogit = logits[j];
                    nextToken = j;
                }
            }

            // Check for end of sequence
            if (nextToken == endToken)
                break;

            generatedTokens.push_back(nextToken);
            tokensGenerated++;

            // Prepare for next iteration: decode the generated token
            batch = llama_batch_get_one(
                &generatedTokens.back(),
                1,
                static_cast<llama_pos>(tokens.size() + tokensGenerated),
                0);

            if (llama_decode(s_impl->ctx, batch) != 0)
                return {"", "Error during token generation"};
        }

        util::Logger::Debug("[Gemma] Generated {} tokens", generatedTokens.size());

        // Step 5: Decode generated tokens back to text using raw C API
        if (generatedTokens.empty())
            return {"", "No tokens generated from model"};

        for (llama_token token : generatedTokens)
        {
            // Use buffer-based API for token_to_piece
            char buffer[128];
            int32_t piece_len = llama_token_to_piece(
                s_impl->model,
                token,
                buffer,
                static_cast<int32_t>(sizeof(buffer))
            );

            if (piece_len > 0 && piece_len <= static_cast<int32_t>(sizeof(buffer)))
            {
                result.append(buffer, piece_len);
            }
        }

        // Clean up result: trim whitespace and control characters
        size_t start = result.find_first_not_of(" \t\n\r");
        size_t end = result.find_last_not_of(" \t\n\r");
        if (start != std::string::npos && end != std::string::npos)
        {
            result = result.substr(start, end - start + 1);
        }

        if (result.empty())
            return {"", "Generated text is empty after cleanup"};

        util::Logger::Info("[Gemma] Inference successful - {} tokens, {} chars",
            generatedTokens.size(), result.length());

        return {result, ""};  // Empty error string indicates success
    }
    catch (const std::exception& e)
    {
        std::string error_msg = std::string("[Gemma] Inference exception: ") + e.what();
        util::Logger::Error("{}", error_msg);
        return {"", error_msg};
    }
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
        // Build prompt for actor extraction
        std::string prompt = R"(Analyze these log messages and extract the unique actors/services mentioned.
Return ONLY a JSON object with "actors" array (list of service/actor names) and "confidence" (0-100).

Log samples:
)";

        int count = 0;
        for (const auto& msg : sampleMessages)
        {
            if (count >= 5) break;  // Limit to 5 samples for context
            prompt += std::to_string(count + 1) + ". " + msg + "\n";
            count++;
        }

        prompt += R"(
Return ONLY valid JSON with this exact structure:
{
  "actors": ["service1", "service2"],
  "confidence": 85
}
)";

        util::Logger::Info("[Gemma] Running actor extraction inference...");
        util::Logger::Debug("[Gemma] Model: {}, Context: {} tokens", s_modelPath,
                          llama_n_ctx(s_impl->ctx));

        // Run actual LLM inference
        auto [response, error] = RunInference(prompt, 200);

        if (!error.empty())
        {
            result.error = error;
            return result;
        }

        // Parse JSON response from LLM
        try
        {
            size_t jsonStart = response.find('{');
            size_t jsonEnd = response.rfind('}');

            if (jsonStart == std::string::npos || jsonEnd == std::string::npos)
            {
                util::Logger::Warn("[Gemma] No JSON found in response: {}", response.substr(0, 100));
                result.error = "No JSON found in LLM response";
                return result;
            }

            std::string jsonStr = response.substr(jsonStart, jsonEnd - jsonStart + 1);
            auto json = nlohmann::json::parse(jsonStr);

            if (json.contains("actors") && json["actors"].is_array())
            {
                for (const auto& actor : json["actors"])
                {
                    if (actor.is_string())
                    {
                        std::string name = actor.get<std::string>();
                        if (!name.empty() && name.length() < 100)  // Sanity check
                            result.actors.insert(name);
                    }
                }
            }

            if (json.contains("confidence") && json["confidence"].is_number())
            {
                result.confidence = json["confidence"].get<int>();
                result.confidence = std::min(100, std::max(0, result.confidence));
            }

            if (result.actors.empty())
                result.error = "No actors extracted by LLM";
            else
                util::Logger::Info("[Gemma] Extracted {} actors", result.actors.size());

            return result;
        }
        catch (const nlohmann::json::exception& e)
        {
            result.error = std::string("JSON parse error: ") + e.what();
            util::Logger::Warn("[Gemma] Failed to parse LLM response: {}", result.error);
            util::Logger::Debug("[Gemma] Response was: {}", response.substr(0, 200));
            return result;
        }
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[Gemma] Actor extraction failed: {}", e.what());
        result.error = std::string("Gemma error: ") + e.what();
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

    // Destructor of Impl will clean up ctx and model
    s_impl.reset();
    llama_backend_free();

    util::Logger::Info("[Gemma] Engine shutdown");
}

bool GemmaInferenceEngine::HasModel()
{
    if (s_modelPath.empty())
    {
        const auto& configPath = config::GetConfig().GetConfigFilePath();
        const auto appDir = std::filesystem::path(configPath).parent_path();
        const auto defaultPath = (appDir / "models" / "gemma-2b.gguf").string();
        return std::filesystem::exists(defaultPath);
    }
    return std::filesystem::exists(s_modelPath);
}

std::string GemmaInferenceEngine::GetModelDownloadUrl()
{
    return "https://huggingface.co/google/gemma-2b-it-gguf";
}

std::string GemmaInferenceEngine::DownloadModel(const std::string& url, const std::string& filename)
{
    const auto& configPath = config::GetConfig().GetConfigFilePath();
    const auto appDir = std::filesystem::path(configPath).parent_path();
    const auto modelsDir = appDir / "models";

    // Create models directory if it doesn't exist
    try
    {
        std::filesystem::create_directories(modelsDir);
    }
    catch (const std::exception& e)
    {
        return std::string("Failed to create models directory: ") + e.what();
    }

    const std::string modelPath = (modelsDir / filename).string();

    // Check if file already exists
    if (std::filesystem::exists(modelPath))
        return "";  // Already downloaded

    const std::string modelDir = modelsDir.string();

    util::Logger::Info("[Gemma] Starting download from HuggingFace...");
    util::Logger::Info("[Gemma] URL: {}", url);
    util::Logger::Info("[Gemma] Destination: {}", modelPath);
    util::Logger::Info("[Gemma] File: {}", filename);

    // Try to download using curl command
    std::string curlCmd = "curl -L -o \"" + modelPath + "\" --progress-bar \"" + url + "\"";

    util::Logger::Info("[Gemma] Running: {}", curlCmd);

    int result = system(curlCmd.c_str());

    if (result != 0)
    {
        // curl failed, try wget as fallback
        util::Logger::Warn("[Gemma] curl failed, trying wget...");
        std::string wgetCmd = "wget -O \"" + modelPath + "\" \"" + url + "\"";
        result = system(wgetCmd.c_str());
    }

    if (result != 0)
    {
        // Manual download instructions
        std::string error = "Download failed. Please download manually:\n\n";
        error += "Option 1: Using curl:\n";
        error += "  mkdir -p " + modelDir + "\n";
        error += "  curl -L -o " + modelPath + " \"" + url + "\"\n\n";
        error += "Option 2: Visit the HuggingFace link and download manually:\n";
        error += "  " + url;

        util::Logger::Error("[Gemma] {}", error);
        return error;
    }

    if (std::filesystem::exists(modelPath))
    {
        util::Logger::Info("[Gemma] Download complete!");
        util::Logger::Info("[Gemma] Model size: {} MB",
                          std::filesystem::file_size(modelPath) / (1024 * 1024));
        s_modelPath = modelPath;  // Update global path to the downloaded model
        return "";  // Success
    }

    return "Download completed but model file not found";
}

GemmaDirectionResult GemmaInferenceEngine::DetectDirections(const std::set<std::string>& sampleMessages)
{
    GemmaDirectionResult result;

    if (!IsAvailable())
    {
        result.error = "Gemma model not available";
        return result;
    }

    try
    {
        util::Logger::Info("[Gemma] Analyzing message directions using AI...");

        // Build prompt for direction analysis
        std::string prompt = "Analyze these log messages and identify direction patterns.\n\n";
        prompt += "Determine:\n";
        prompt += "1. Which field contains SENDERS (from, source, sender, client, etc.)\n";
        prompt += "2. Which field contains RECEIVERS (to, dest, receiver, server, etc.)\n";
        prompt += "3. Keywords that indicate SENDING direction (send, request, tx, out, etc.)\n";
        prompt += "4. Keywords that indicate RECEIVING direction (recv, response, rx, in, etc.)\n\n";
        prompt += "Sample messages:\n";

        int lineNum = 1;
        for (const auto& msg : sampleMessages)
        {
            if (lineNum > 8) break;
            prompt += std::to_string(lineNum) + ". " + msg + "\n";
            ++lineNum;
        }

        prompt += "\nReturn ONLY a JSON object:\n";
        prompt += "{\n";
        prompt += "  \"sender_field\": \"from_service\",\n";
        prompt += "  \"receiver_field\": \"to_service\",\n";
        prompt += "  \"sender_keywords\": [\"from\", \"sender\", \"source\"],\n";
        prompt += "  \"receiver_keywords\": [\"to\", \"receiver\", \"dest\"],\n";
        prompt += "  \"direction_keywords\": [\"send\", \"request\", \"response\"],\n";
        prompt += "  \"confidence\": 85\n";
        prompt += "}\n";

        util::Logger::Debug("[Gemma] Direction analysis prompt ({} chars)", prompt.length());

        // Run actual LLM inference for direction analysis
        util::Logger::Info("[Gemma] Running direction detection inference...");
        auto [response, error] = RunInference(prompt, 250);

        if (!error.empty())
        {
            result.error = error;
            return result;
        }

        util::Logger::Debug("[Gemma] Direction analysis response:\n{}", response.substr(0, 300));

        // Parse JSON response
        try
        {
            size_t jsonStart = response.find('{');
            size_t jsonEnd = response.rfind('}');

            if (jsonStart != std::string::npos && jsonEnd != std::string::npos)
            {
                std::string jsonStr = response.substr(jsonStart, jsonEnd - jsonStart + 1);
                auto json = nlohmann::json::parse(jsonStr);

                // Create DirectionPattern and populate it
                DirectionPattern pattern;

                if (json.contains("sender_field"))
                    pattern.senderField = json["sender_field"].get<std::string>();

                if (json.contains("receiver_field"))
                    pattern.receiverField = json["receiver_field"].get<std::string>();

                if (json.contains("sender_keywords") && json["sender_keywords"].is_array())
                {
                    for (const auto& kw : json["sender_keywords"])
                    {
                        if (kw.is_string())
                            pattern.senderKeywords.insert(kw.get<std::string>());
                    }
                }

                if (json.contains("receiver_keywords") && json["receiver_keywords"].is_array())
                {
                    for (const auto& kw : json["receiver_keywords"])
                    {
                        if (kw.is_string())
                            pattern.receiverKeywords.insert(kw.get<std::string>());
                    }
                }

                if (json.contains("direction_keywords") && json["direction_keywords"].is_array())
                {
                    for (const auto& kw : json["direction_keywords"])
                    {
                        if (kw.is_string())
                            pattern.directionKeywords.insert(kw.get<std::string>());
                    }
                }

                if (json.contains("confidence"))
                {
                    pattern.confidence = std::min(100, std::max(0, json["confidence"].get<int>()));
                }

                pattern.description = pattern.senderField + " → " + pattern.receiverField;

                util::Logger::Info("[Gemma] Direction detection complete: sender={}, receiver={}, confidence={}",
                                 pattern.senderField, pattern.receiverField, pattern.confidence);

                result.pattern = pattern;  // Store in optional
            }
            else
            {
                result.error = "Invalid JSON in response";
            }
        }
        catch (const nlohmann::json::exception& e)
        {
            result.error = std::string("JSON parse error: ") + e.what();
            util::Logger::Warn("[Gemma] {}", result.error);
        }

        return result;
    }
    catch (const std::exception& e)
    {
        util::Logger::Error("[Gemma] Direction detection failed: {}", e.what());
        result.error = std::string("Gemma direction error: ") + e.what();
        return result;
    }
}

} // namespace ai
