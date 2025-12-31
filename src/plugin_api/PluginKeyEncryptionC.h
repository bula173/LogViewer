// C-style key encryption API for plugins. Header-only; application registers callbacks.
#pragma once

#include <cstdlib>
#include <cstring>

extern "C" {

typedef char* (*PluginEncryptFn)(const char* input); // returns malloc'd string or nullptr
typedef char* (*PluginDecryptFn)(const char* input); // returns malloc'd string or nullptr

// Registration
void PluginKeyEncryption_Register(PluginEncryptFn encFn, PluginDecryptFn decFn);

// Helpers used by plugins
char* PluginKeyEncryption_Encrypt(const char* input);
char* PluginKeyEncryption_Decrypt(const char* input);
void PluginKeyEncryption_FreeString(char* s);

static PluginEncryptFn g_plugin_encrypt_fn = nullptr;
static PluginDecryptFn g_plugin_decrypt_fn = nullptr;

inline void PluginKeyEncryption_Register(PluginEncryptFn encFn, PluginDecryptFn decFn)
{
    g_plugin_encrypt_fn = encFn;
    g_plugin_decrypt_fn = decFn;
}

inline char* PluginKeyEncryption_Encrypt(const char* input)
{
    if (!input) return nullptr;
    if (g_plugin_encrypt_fn) return g_plugin_encrypt_fn(input);
    size_t n = std::strlen(input);
    char* out = (char*)std::malloc(n + 1);
    if (!out) return nullptr;
    std::memcpy(out, input, n + 1);
    return out;
}

inline char* PluginKeyEncryption_Decrypt(const char* input)
{
    if (!input) return nullptr;
    if (g_plugin_decrypt_fn) return g_plugin_decrypt_fn(input);
    size_t n = std::strlen(input);
    char* out = (char*)std::malloc(n + 1);
    if (!out) return nullptr;
    std::memcpy(out, input, n + 1);
    return out;
}

inline void PluginKeyEncryption_FreeString(char* s)
{
    if (s) std::free(s);
}

} // extern "C"
