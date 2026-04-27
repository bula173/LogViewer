#include "KeyEncryption.hpp"
#include <cstring>
#include <string_view>

namespace util
{

// Prefix to identify encrypted values
static constexpr const char* ENCRYPTED_PREFIX = "ENC:";

// ---------------------------------------------------------------------------
// Key obfuscation: static XOR + hex encoding (all platforms)
//
// Purpose: prevent API keys from being stored as plaintext in config files.
// This is intentional *obfuscation*, not strong cryptography.  Strong
// per-user encryption (OS keychain / DPAPI) is a future enhancement.
//
// Why NOT Windows DPAPI (CryptProtectData / CryptUnprotectData):
//   CryptProtectData is imported by credential stealers and ransomware;
//   ML-based AV engines (Defender, Trapmine) assign a high-risk score to
//   any binary that imports Crypt32!CryptProtectData while also importing
//   LoadLibraryExW and performing network I/O — exactly our profile.
//
// Why a static compile-time key is acceptable here:
//   The key is visible in the binary (no security through obscurity), but
//   that is fine — this is obfuscation, not encryption.  The threat model
//   is "casual inspection of config.json", not "determined attacker with
//   binary access".
// ---------------------------------------------------------------------------
static constexpr std::string_view kObfKey = "LogViewer-2026-ObfuscationKey";

static std::string XorHexEncode(const std::string& input)
{
    std::string out;
    out.reserve(input.size() * 2);
    constexpr char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < input.size(); ++i)
    {
        const auto b = static_cast<unsigned char>(input[i]) ^
                       static_cast<unsigned char>(kObfKey[i % kObfKey.size()]);
        out.push_back(hex[(b >> 4) & 0xF]);
        out.push_back(hex[b & 0xF]);
    }
    return out;
}

static std::string XorHexDecode(const std::string& hex)
{
    if (hex.size() % 2 != 0) return {};
    std::string out;
    out.reserve(hex.size() / 2);
    for (size_t i = 0; i < hex.size(); i += 2)
    {
        auto h = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        const int hi = h(hex[i]);
        const int lo = h(hex[i + 1]);
        if (hi < 0 || lo < 0) return {};
        const auto b = static_cast<unsigned char>((hi << 4) | lo);
        out.push_back(static_cast<char>(
            b ^ static_cast<unsigned char>(kObfKey[(i / 2) % kObfKey.size()])));
    }
    return out;
}

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::string KeyEncryption::Encrypt(const std::string& plaintext)
{
    if (plaintext.empty()) return {};
    return std::string(ENCRYPTED_PREFIX) + XorHexEncode(plaintext);
}

std::string KeyEncryption::Decrypt(const std::string& ciphertext)
{
    if (ciphertext.empty()) return {};
    if (ciphertext.find(ENCRYPTED_PREFIX) != 0)
        return ciphertext;  // not obfuscated — return as-is
    return XorHexDecode(ciphertext.substr(strlen(ENCRYPTED_PREFIX)));
}

bool KeyEncryption::IsEncrypted(const std::string& value)
{
    return value.find(ENCRYPTED_PREFIX) == 0;
}

} // namespace util
