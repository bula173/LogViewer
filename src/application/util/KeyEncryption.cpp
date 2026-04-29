#include "KeyEncryption.hpp"
#include <cstring>

namespace util
{

// Prefix to identify encoded values.
// Note: old XOR-based values used "ENC:" — those are no longer recognised and
// will be treated as plaintext (users will need to re-enter their API key once).
static constexpr const char* ENCODED_PREFIX = "B64:";

// ---------------------------------------------------------------------------
// Key encoding: Base64 (all platforms)
//
// Purpose: prevent API keys from being stored as plaintext in config files.
// This is intentional *encoding*, not strong cryptography. The threat model
// is "casual inspection of config.json", not "determined attacker with
// binary access".
//
// Base64 is used instead of XOR+static-key obfuscation because XOR with an
// embedded key is a recognised malware obfuscation pattern that triggers
// ML-based AV engines (e.g. Trapmine, Microsoft Wacatac.B!ml) when combined
// with dynamic library loading and network I/O — all of which this app
// legitimately uses for its plugin system and AI features.
// ---------------------------------------------------------------------------

static const char kBase64Chars[] =
    "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string Base64Encode(const std::string& input)
{
    std::string out;
    out.reserve(((input.size() + 2) / 3) * 4);
    const auto* data = reinterpret_cast<const unsigned char*>(input.data());
    const size_t len  = input.size();
    for (size_t i = 0; i < len; i += 3)
    {
        const unsigned int b0 = data[i];
        const unsigned int b1 = (i + 1 < len) ? data[i + 1] : 0u;
        const unsigned int b2 = (i + 2 < len) ? data[i + 2] : 0u;
        out.push_back(kBase64Chars[(b0 >> 2) & 0x3F]);
        out.push_back(kBase64Chars[((b0 & 0x03) << 4) | ((b1 >> 4) & 0x0F)]);
        out.push_back((i + 1 < len) ? kBase64Chars[((b1 & 0x0F) << 2) | ((b2 >> 6) & 0x03)] : '=');
        out.push_back((i + 2 < len) ? kBase64Chars[b2 & 0x3F] : '=');
    }
    return out;
}

static std::string Base64Decode(const std::string& input)
{
    std::string out;
    out.reserve((input.size() / 4) * 3);
    int val  = 0;
    int bits = -8;
    for (const char c : input)
    {
        if (c == '=') break;
        const char* pos = std::strchr(kBase64Chars, static_cast<unsigned char>(c));
        if (!pos) return {};  // invalid character — corrupt value
        val   = (val << 6) | static_cast<int>(pos - kBase64Chars);
        bits += 6;
        if (bits >= 0)
        {
            out.push_back(static_cast<char>((val >> bits) & 0xFF));
            bits -= 8;
        }
    }
    return out;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::string KeyEncryption::Encrypt(const std::string& plaintext)
{
    if (plaintext.empty()) return {};
    return std::string(ENCODED_PREFIX) + Base64Encode(plaintext);
}

std::string KeyEncryption::Decrypt(const std::string& ciphertext)
{
    if (ciphertext.empty()) return {};
    if (ciphertext.find(ENCODED_PREFIX) != 0)
        return ciphertext;  // not encoded — return as-is (plaintext)
    return Base64Decode(ciphertext.substr(strlen(ENCODED_PREFIX)));
}

bool KeyEncryption::IsEncrypted(const std::string& value)
{
    return value.find(ENCODED_PREFIX) == 0;
}

} // namespace util
