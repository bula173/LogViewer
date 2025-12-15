# Cloud AI Provider Integration

The application now supports multiple AI providers including cloud services (ChatGPT, Claude, Gemini, Grok).

## Supported Providers

### Local Providers (No API Key Required)
- **Ollama** - `http://localhost:11434` - Free, open-source, runs locally
- **LM Studio** - `http://localhost:1234` - Free, runs any GGUF model locally

### Cloud Providers (API Key Required)

#### OpenAI / ChatGPT
- **Provider Code**: `openai`
- **Base URL**: `https://api.openai.com/v1`
- **Models**: `gpt-4`, `gpt-4-turbo`, `gpt-3.5-turbo`
- **Get API Key**: https://platform.openai.com/api-keys
- **Pricing**: Pay per token

#### Anthropic / Claude
- **Provider Code**: `anthropic`
- **Base URL**: `https://api.anthropic.com/v1`
- **Models**: `claude-3-opus-20240229`, `claude-3-sonnet-20240229`, `claude-3-haiku-20240307`
- **Get API Key**: https://console.anthropic.com/
- **Pricing**: Pay per token

#### Google / Gemini
- **Provider Code**: `google`
- **Base URL**: `https://generativelanguage.googleapis.com/v1`
- **Models**: `gemini-pro`, `gemini-1.5-pro`, `gemini-1.5-flash`
- **Get API Key**: https://ai.google.dev/
- **Pricing**: Free tier available, then pay per token

#### xAI / Grok
- **Provider Code**: `xai`
- **Base URL**: `https://api.x.ai/v1`
- **Models**: `grok-beta`
- **Get API Key**: https://x.ai/api
- **Pricing**: Pay per token

## Configuration

### Via UI (Recommended)
1. Open **Tools → Preferences**
2. Go to **AI Configuration** tab
3. Select your provider from dropdown
4. Enter API key (for cloud providers)
5. Adjust model name if needed
6. Click **Save**

### Via config.json
```json
{
  "aiConfig": {
    "provider": "openai",
    "apiKey": "sk-your-api-key-here",
    "baseUrl": "https://api.openai.com/v1",
    "defaultModel": "gpt-4"
  }
}
```

## API Authentication

### OpenAI, xAI (Bearer Token)
```
Authorization: Bearer YOUR_API_KEY
```

### Anthropic (x-api-key Header)
```
x-api-key: YOUR_API_KEY
anthropic-version: 2023-06-01
```

### Google (API Key in URL)
```
https://generativelanguage.googleapis.com/v1/models/gemini-pro:generateContent?key=YOUR_API_KEY
```

## Implementation Status

### ✅ Completed
- Configuration system for all providers
- UI for provider selection
- API key field (password-protected)
- Base URL configuration
- Model name configuration
- Config save/load

### 🔄 To Implement in OllamaClient
1. **Add API key to HTTP headers** based on provider
2. **Different request formats** for each provider:
   - OpenAI: `{"model": "gpt-4", "messages": [...]}`
   - Anthropic: `{"model": "claude-3", "messages": [...], "max_tokens": 4096}`
   - Google: `{"contents": [{"parts": [{"text": "..."}]}]}`
3. **Parse different response formats**
4. **Handle provider-specific errors**

## Testing Cloud Providers

### 1. Get API Key
Sign up for your chosen provider and generate an API key.

### 2. Configure in App
```
Provider: OpenAI / ChatGPT
API Key: sk-proj-xxxxxxxxxxxxx
Model: gpt-4
```

### 3. Test with Simple Prompt
- Load a log file
- Go to AI Analysis tab
- Select "Summary" analysis
- Click "Analyze Logs"

## Cost Considerations

Cloud providers charge per token:
- **Input tokens**: Your prompts + log context
- **Output tokens**: AI responses

Example costs (approximate):
- GPT-4: $0.03/1K input tokens, $0.06/1K output tokens
- Claude 3 Opus: $0.015/1K input tokens, $0.075/1K output tokens
- Gemini Pro: Free up to 60 requests/minute
- Grok: Pricing TBD

**Tip**: Start with free local models (Ollama) for testing, use cloud providers for production.

## Security Notes

⚠️ **Important**: API keys are stored in `config.json` in plain text.

### Best Practices:
1. Never commit `config.json` with API keys to git
2. Use environment variables for production
3. Rotate keys regularly
4. Use API key restrictions (IP allowlists, rate limits)
5. Monitor usage to detect unauthorized access

### Future Enhancement:
- Encrypt API keys in config file
- Support for environment variables
- Keychain/credential manager integration

## Troubleshooting

### "API key invalid"
- Check you copied the full key
- Verify key hasn't expired
- Ensure key has correct permissions

### "Rate limit exceeded"
- Wait a few minutes
- Upgrade to paid tier
- Use local provider instead

### "Model not found"
- Check model name spelling
- Verify model is available in your region
- Some models require approval

## Next Steps

To fully implement cloud provider support, update `OllamaClient.cpp`:

1. Rename `OllamaClient` → `LLMClient` (or keep both)
2. Add provider-specific request builders
3. Add provider-specific response parsers
4. Add authentication headers based on provider
5. Handle different error formats

See `src/plugins/ai/LLMClient.hpp` for a proposed interface.
