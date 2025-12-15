# OpenAI Integration

## Overview

The OpenAI integration provides access to ChatGPT and other OpenAI models through the OpenAI API. This implementation follows the same pattern as other AI providers in the application.

## Implementation Details

### Class: `OpenAIClient`

**Location**: `src/plugins/ai/OpenAIClient.{hpp,cpp}`

**Dependencies**:
- CURL for HTTP communication
- nlohmann/json for JSON serialization/deserialization
- Bearer token authentication

### Key Features

1. **Bearer Token Authentication**
   - Uses API key with `Authorization: Bearer <token>` header
   - Configurable base URL (defaults to OpenAI API endpoint)

2. **Configurable Timeout**
   - Uses `config.aiTimeoutSeconds` from application configuration
   - Supports 30-3600 seconds range

3. **Error Handling**
   - CURL errors are caught and logged
   - JSON parsing errors are handled gracefully
   - API errors return descriptive error messages

4. **Health Check**
   - `IsAvailable()` performs a minimal API request to validate connectivity
   - Tests API key validity before making actual requests

## API Endpoints

### Chat Completions
- **Endpoint**: `/v1/chat/completions`
- **Method**: POST
- **Request Format**:
```json
{
  "model": "gpt-4",
  "messages": [
    {
      "role": "user",
      "content": "Your prompt here"
    }
  ],
  "temperature": 0.7
}
```

- **Response Format**:
```json
{
  "choices": [
    {
      "message": {
        "role": "assistant",
        "content": "AI response here"
      }
    }
  ]
}
```

## Configuration

### Required Settings
```json
{
  "aiProvider": "openai",
  "openaiApiKey": "sk-...",
  "openaiModel": "gpt-4",
  "openaiBaseUrl": "https://api.openai.com",
  "aiTimeoutSeconds": 60
}
```

### Supported Models
- gpt-4 (default)
- gpt-4-turbo
- gpt-3.5-turbo
- Any other OpenAI-compatible model

## Usage Example

```cpp
#include "ai/OpenAIClient.hpp"

// Initialize client
ai::OpenAIClient client(
    "sk-your-api-key",
    "gpt-4",
    "https://api.openai.com"
);

// Check availability
if (client.IsAvailable()) {
    // Send prompt
    std::string response = client.SendPrompt("What is C++20?", nullptr);
    std::cout << response << std::endl;
}
```

## HTTP Implementation

### CURL Setup
1. Initialize CURL handle
2. Set URL to `baseUrl + endpoint`
3. Configure POST request with JSON body
4. Add headers:
   - `Content-Type: application/json`
   - `Authorization: Bearer <apiKey>`
5. Set timeout from config
6. Execute request and capture response
7. Clean up resources

### WriteCallback
- Custom callback to capture HTTP response data
- Appends data to std::string buffer
- Returns total bytes processed

## Error Handling

### Connection Errors
- CURL initialization failures
- Network timeouts
- HTTP request failures
- All throw `std::runtime_error` with descriptive messages

### API Errors
- Invalid API key → Authentication failure
- Rate limiting → HTTP 429 errors
- Invalid model → Model not found errors
- All logged via `util::Logger`

### Response Parsing Errors
- Invalid JSON → Returns error message
- Missing fields → Returns error message
- Malformed response → Returns error message

## Security Considerations

1. **API Key Storage**
   - Never hardcode API keys in source
   - Load from configuration files
   - Keep config files out of version control
   - Use environment variables in CI/CD

2. **HTTPS**
   - Always use HTTPS endpoint
   - CURL validates SSL certificates by default

3. **Timeout Configuration**
   - Prevents hanging requests
   - User-configurable for different network conditions

## Testing

### Manual Testing
1. Set valid API key in `config.json`
2. Build and run application
3. Select OpenAI as AI provider
4. Send test prompts through UI

### Unit Testing
- Mock CURL responses for testing
- Test JSON parsing with various response formats
- Verify error handling paths
- Test timeout behavior

## Troubleshooting

### Common Issues

**"OpenAI API request failed: Could not resolve host"**
- Check internet connectivity
- Verify base URL is correct
- Check firewall/proxy settings

**"Authentication failed"**
- Verify API key is valid
- Check if API key has been revoked
- Ensure no extra spaces in API key

**"Timeout"**
- Increase `aiTimeoutSeconds` in config
- Check network latency
- Verify OpenAI API status

**"Invalid response from OpenAI"**
- Check OpenAI API status page
- Verify model name is correct
- Check API rate limits

## Integration with Factory Pattern

The OpenAI client is instantiated through `AIProviderFactory`:

```cpp
std::unique_ptr<IAIProvider> provider = 
    AIProviderFactory::CreateProvider("openai");
```

The factory reads configuration and creates the appropriate client with proper settings.

## Future Enhancements

1. **Streaming Support**
   - Implement SSE (Server-Sent Events) for streaming responses
   - Real-time token-by-token updates

2. **Additional Parameters**
   - Support for system messages
   - Configurable temperature
   - max_tokens parameter
   - top_p, frequency_penalty, presence_penalty

3. **Model Management**
   - List available models via API
   - Model capability detection
   - Cost estimation

4. **Advanced Features**
   - Function calling support
   - Multi-turn conversations with context
   - Image input support (GPT-4 Vision)

## References

- [OpenAI API Documentation](https://platform.openai.com/docs/api-reference)
- [CURL Documentation](https://curl.se/libcurl/c/)
- [nlohmann/json Documentation](https://github.com/nlohmann/json)
- [LogViewer AI Integration Guide](DEVELOPMENT.md#ai-integration-development)
