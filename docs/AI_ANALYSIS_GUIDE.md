# AI Log Analysis Setup Guide

## Overview

Your LogViewer now includes local AI-powered log analysis using Ollama. This keeps all your log data private on your machine.

## Features

The AI can analyze your logs in several ways:

1. **Summary** - Quick overview of the log session
2. **Error Analysis** - Deep dive into errors and their causes
3. **Pattern Detection** - Find unusual patterns and anomalies
4. **Root Cause Analysis** - Identify why problems occurred
5. **Timeline** - Narrative walkthrough of events

## Installation Steps

### 1. Install Ollama

**macOS:**
```bash
brew install ollama
```

Or download from: https://ollama.ai

**Linux:**
```bash
curl -fsSL https://ollama.ai/install.sh | sh
```

**Windows:**
Download installer from: https://ollama.ai

### 2. Start Ollama Service

**macOS/Linux:**
```bash
ollama serve
```

**Windows:**
Ollama runs automatically as a Windows service after installation. No manual start needed!

This starts the local server on `http://localhost:11434`

### 3. Download AI Model

**Recommended for log analysis:**
```bash
ollama pull qwen2.5-coder:7b
```

**Alternative models:**
```bash
# Fast and lightweight (good for quick summaries)
ollama pull llama3.2         # 2GB

# Best quality (slower, needs more RAM)
ollama pull deepseek-coder-v2:16b  # 9GB

# Balanced option
ollama pull llama3.1:8b      # 4.7GB
```

See "Model Selection" section below for comparison.

### 4. Using AI Analysis

1. Load a log file in LogViewer
2. Click on the "AI Analysis" tab in the main content area
3. **(New!)** Select AI model from dropdown (default: Qwen 2.5 Coder 7B)
4. Select analysis type (Summary, Error Analysis, etc.)
5. Set max events to analyze (default: 5,000)
6. Click "Analyze Logs"
7. Wait for results (typically 10-30 seconds)

**Note:** The application uses filter-aware analysis. If you have active filters (e.g., showing only ERROR events), the AI will analyze only the filtered results.

### 5. Configure Timeout (For Slow Machines)

1. Go to Edit → Config → AI tab
2. Adjust "Request Timeout" (default: 300 seconds)
3. For slow machines or large analyses, increase to 600-900 seconds
4. Click Save

## Model Selection

LogViewer includes a model selector in the AI Analysis panel. Choose based on your needs:

| Model | Best For | Speed | Quality | Size |
|-------|----------|-------|---------|------|
| **Qwen 2.5 Coder 7B** | Technical logs, errors | ⚡⚡⚡ | ⭐⭐⭐⭐⭐ | 4.7GB |
| Llama 3.2 3B | Quick summaries | ⚡⚡⚡⚡ | ⭐⭐⭐ | 2GB |
| Llama 3.1 8B | General analysis | ⚡⚡⚡ | ⭐⭐⭐⭐ | 4.7GB |
| DeepSeek V2 16B | Complex root cause | ⚡⚡ | ⭐⭐⭐⭐⭐ | 9GB |
| Mistral 7B | Alternative option | ⚡⚡⚡ | ⭐⭐⭐⭐ | 4.1GB |
| Llama 3.2 1B | Very fast, basic | ⚡⚡⚡⚡⚡ | ⭐⭐ | 1.3GB |

**Note:** Models must be downloaded first using `ollama pull <model-name>`.

## Configuration

All AI settings can be configured through the UI:

1. **Edit → Config → AI tab:**
   - AI Provider: Choose between Ollama, LM Studio, OpenAI, Anthropic, Google, xAI
   - API Key: For cloud providers (not needed for local Ollama/LM Studio)
   - Base URL: Default `http://localhost:11434` for Ollama
   - Default Model: e.g., `qwen2.5-coder:7b`
   - Request Timeout: Adjust for slow machines (30-3600 seconds)

2. **Edit → Config → General tab:**
   - Type Filter Field: Set which field to use for filtering/coloring (e.g., "level", "severity")

Configuration is saved to `~/Library/Application Support/LogViewer/config.json` (macOS) or equivalent on other platforms.

## Troubleshooting

### "Ollama not available" status

- Ensure Ollama is running: `ollama serve`
- Check if the port is correct (default: 11434)
- Verify model is downloaded: `ollama list`

### Slow analysis or timeouts

- **Increase timeout**: Edit → Config → AI tab → Request Timeout (increase to 600+ seconds)
- Reduce max events to analyze (default cap is 5,000 with intelligent sampling)
- Use a smaller model like `llama3.2:1b`
- Ensure your CPU/GPU has enough resources
- Check system load (close other heavy applications)

### Connection errors

- Check firewall settings
- Verify Ollama is listening on localhost:11434
- Try restarting Ollama service

## Privacy

All analysis happens locally on your machine. No log data is sent to external servers.

## Performance

- Initial analysis may take 30-60 seconds as the model loads
- Subsequent analyses are faster (10-20 seconds)
- Analyzing 100 events typically takes 15-30 seconds
- Memory usage: 4-8GB depending on model

## Architecture

```
LogViewer
    ├── AIAnalysisPanel (Qt UI)
    ├── LogAnalyzer (Formats logs, builds prompts)
    ├── OllamaClient (HTTP communication)
    └── Ollama Server (Local LLM)
```

## Future Enhancements

Potential additions:
- Real-time analysis as logs load
- Custom analysis prompts
- Export analysis reports
- Pattern learning across sessions
- Integration with search/filters
