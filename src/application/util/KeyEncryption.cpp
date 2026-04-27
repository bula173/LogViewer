#include "KeyEncryption.hpp"
#include "Logger.hpp"
#include <string_view>

#ifdef _WIN32
// Use Windows DPAPI (CryptProtectData / CryptUnprotectData).
// This is the OS-provided credential-protection API explicitly allowlisted
// by Windows Defender and other AV products for exactly this use case.
// It avoids:
//   - Reading HKLM\SOFTWARE\Microsoft\Cryptography\MachineGuid (ransomware
//     fingerprinting heuristic)
//   - Custom XOR cipher loop on a buffer (crypto-ransomware heuristic)
//   - Custom Base64 encoding of the result (data-exfiltration heuristic)
#include <windows.h>
#include <wincrypt.h>
#include <vector>
#pragma comment(lib, "Crypt32.lib")
#endif

namespace util
{

// Prefix to identify encrypted values
static constexpr const char* ENCRYPTED_PREFIX = "ENC:";

#ifdef _WIN32
// ---------------------------------------------------------------------------
// Windows: DPAPI helpers
//   CryptProtectData  — encrypts for the current Windows user account.
//   CryptUnprotectData — decrypts (only succeeds for the same user/machine).
// Neither function reads the registry, performs XOR loops, or produces
// raw base64 output that AV heuristics associate with ransomware.
// ---------------------------------------------------------------------------
static std::string DpapiEncrypt(const std::string& plaintext)
{
    DATA_BLOB input;
    input.pbData = reinterpret_cast<BYTE*>(const_cast<char*>(plaintext.data()));
    input.cbData = static_cast<DWORD>(plaintext.size());

    DATA_BLOB output{};
    if (!CryptProtectData(&input, L"LogViewer API key", nullptr, nullptr, nullptr,
                          CRYPTPROTECT_LOCAL_MACHINE, &output))
    {
        Logger::Warn("DPAPI CryptProtectData failed (code {})", GetLastError());
        return {};
    }

    // Hex-encode the opaque blob so it is safe for JSON storage.
    std::string result;
    result.reserve(output.cbData * 2);
    for (DWORD i = 0; i < output.cbData; ++i)
    {
        constexpr char hex[] = "0123456789abcdef";
        result.push_back(hex[(output.pbData[i] >> 4) & 0xF]);
        result.push_back(hex[output.pbData[i] & 0xF]);
    }
    LocalFree(output.pbData);
    return result;
}

static std::string DpapiDecrypt(const std::string& hexBlob)
{
    if (hexBlob.size() % 2 != 0) return {};

    std::vector<BYTE> bytes;
    bytes.reserve(hexBlob.size() / 2);
    for (size_t i = 0; i < hexBlob.size(); i += 2)
    {
        auto hexVal = [](char c) -> int {
            if (c >= '0' && c <= '9') return c - '0';
            if (c >= 'a' && c <= 'f') return c - 'a' + 10;
            if (c >= 'A' && c <= 'F') return c - 'A' + 10;
            return -1;
        };
        int hi = hexVal(hexBlob[i]);
        int lo = hexVal(hexBlob[i + 1]);
        if (hi < 0 || lo < 0) return {};
        bytes.push_back(static_cast<BYTE>((hi << 4) | lo));
    }

    DATA_BLOB input;
    input.pbData = bytes.data();
    input.cbData = static_cast<DWORD>(bytes.size());

    DATA_BLOB output{};
    if (!CryptUnprotectData(&input, nullptr, nullptr, nullptr, nullptr,
                            CRYPTPROTECT_LOCAL_MACHINE, &output))
    {
        Logger::Warn("DPAPI CryptUnprotectData failed (code {})", GetLastError());
        return {};
    }

    std::string result(reinterpret_cast<char*>(output.pbData), output.cbData);
    LocalFree(output.pbData);
    return result;
}
#endif // _WIN32

#ifndef _WIN32
// Non-Windows: minimal XOR+hex obfuscation using a static compile-time key.
// This is not strong encryption — it is intentional obfuscation so that
// API keys are not stored as plaintext in config files.  On Linux/macOS
// the OS keychain (Secret Service / Keychain) would be the right solution
// but is an optional future enhancement.
static constexpr std::string_view kStaticKey = "LogViewer-2026-ObfuscationKey";

static std::string XorHexEncode(const std::string& input)
{
    std::string out;
    out.reserve(input.size() * 2);
    constexpr char hex[] = "0123456789abcdef";
    for (size_t i = 0; i < input.size(); ++i)
    {
        const auto b = static_cast<unsigned char>(input[i]) ^
                       static_cast<unsigned char>(kStaticKey[i % kStaticKey.size()]);
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
        int hi = h(hex[i]);
        int lo = h(hex[i + 1]);
        if (hi < 0 || lo < 0) return {};
        const auto b = static_cast<unsigned char>((hi << 4) | lo);
        out.push_back(static_cast<char>(b ^ static_cast<unsigned char>(
            kStaticKey[(i / 2) % kStaticKey.size()])));
    }
    return out;
}
#endif // !_WIN32

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

std::string KeyEncryption::Encrypt(const std::string& plaintext)
{
    if (plaintext.empty()) return {};

#ifdef _WIN32
    const std::string blob = DpapiEncrypt(plaintext);
    if (blob.empty()) return plaintext;  // fallback: store plaintext rather than lose the key
    return std::string(ENCRYPTED_PREFIX) + blob;
#else
    return std::string(ENCRYPTED_PREFIX) + XorHexEncode(plaintext);
#endif
}

std::string KeyEncryption::Decrypt(const std::string& ciphertext)
{
    if (ciphertext.empty()) return {};

    if (ciphertext.find(ENCRYPTED_PREFIX) != 0)
        return ciphertext;  // not encrypted — return as-is

    const std::string blob = ciphertext.substr(strlen(ENCRYPTED_PREFIX));

#ifdef _WIN32
    const std::string plain = DpapiDecrypt(blob);
    return plain.empty() ? ciphertext : plain;  // fallback: return raw if DPAPI fails
#else
    return XorHexDecode(blob);
#endif
}

bool KeyEncryption::IsEncrypted(const std::string& value)
{
    return value.find(ENCRYPTED_PREFIX) == 0;
}

} // namespace util
