#include "KeyEncryption.hpp"
#include "Logger.hpp"
#include <array>
#include <sstream>
#include <iomanip>
#include <algorithm>

#ifdef __APPLE__
#include <sys/sysctl.h>
#elif defined(_WIN32)
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
#pragma comment(lib, "Crypt32.lib")
#else
#include <fstream>
#include <unistd.h>
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


std::string KeyEncryption::GetDeviceKey()
{
    // Generate a device-specific key for XOR cipher
    std::string deviceId;
    
#ifdef __APPLE__
    // macOS: Use hardware UUID
    char uuid[37] = {0};
    size_t len = sizeof(uuid) - 1;
    if (sysctlbyname("kern.uuid", uuid, &len, nullptr, 0) == 0)
    {
        deviceId = uuid;
    }
#elif defined(_WIN32)
    // Windows: Use machine GUID from registry
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, 
                      "SOFTWARE\\Microsoft\\Cryptography",
                      0, KEY_READ, &hKey) == ERROR_SUCCESS)
    {
        char guid[256] = {0};
        DWORD guidLen = sizeof(guid);
        if (RegQueryValueExA(hKey, "MachineGuid", nullptr, nullptr,
                            reinterpret_cast<LPBYTE>(guid), &guidLen) == ERROR_SUCCESS)
        {
            deviceId = guid;
        }
        RegCloseKey(hKey);
    }
#else
    // Linux: Try machine-id
    std::ifstream machineId("/etc/machine-id");
    if (machineId.is_open())
    {
        std::getline(machineId, deviceId);
    }
    else
    {
        // Fallback to hostname
        char hostname[256] = {0};
        if (gethostname(hostname, sizeof(hostname)) == 0)
        {
            deviceId = hostname;
        }
    }
#endif
    
    if (deviceId.empty())
    {
        // Fallback: use a static key (less secure but better than nothing)
        deviceId = "LogViewer-Default-Key-2024";
        Logger::Warn("Could not determine device ID, using default encryption key");
    }
    
    return deviceId;
}

std::string KeyEncryption::XorCipher(const std::string& input, const std::string& key)
{
    if (key.empty())
        return input;
    
    std::string output = input;
    for (size_t i = 0; i < input.size(); ++i)
    {
        output[i] = input[i] ^ key[i % key.size()];
    }
    return output;
}

std::string KeyEncryption::Base64Encode(const std::string& input)
{
    static const char* base64_chars = 
        "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
        "abcdefghijklmnopqrstuvwxyz"
        "0123456789+/";
    
    std::string output;
    int val = 0;
    int valb = -6;
    
    for (char c : input)
    {
        val = (val << 8) + static_cast<unsigned char>(c);
        valb += 8;
        while (valb >= 0)
        {
            output.push_back(base64_chars[(val >> valb) & 0x3F]);
            valb -= 6;
        }
    }
    
    if (valb > -6)
        output.push_back(base64_chars[((val << 8) >> (valb + 8)) & 0x3F]);
    
    while (output.size() % 4)
        output.push_back('=');
    
    return output;
}

std::string KeyEncryption::Base64Decode(const std::string& input)
{
    static const unsigned char base64_table[256] = {
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 62, 64, 64, 64, 63,
        52, 53, 54, 55, 56, 57, 58, 59, 60, 61, 64, 64, 64, 64, 64, 64,
        64,  0,  1,  2,  3,  4,  5,  6,  7,  8,  9, 10, 11, 12, 13, 14,
        15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 64, 64, 64, 64, 64,
        64, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40,
        41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64,
        64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64, 64
    };
    
    std::string output;
    int val = 0;
    int valb = -8;
    
    for (const char ch : input)
    {
        const auto c = static_cast<unsigned char>(ch);
        if (base64_table[c] == 64)
            break;
        val = (val << 6) + base64_table[c];
        valb += 6;
        if (valb >= 0)
        {
            output.push_back(char((val >> valb) & 0xFF));
            valb -= 8;
        }
    }
    
    return output;
}

std::string KeyEncryption::Encrypt(const std::string& plaintext)
{
    if (plaintext.empty())
        return "";
    
    // XOR with device key
    const std::string deviceKey = GetDeviceKey();
    const std::string encrypted = XorCipher(plaintext, deviceKey);
    
    // Base64 encode and add prefix
    return std::string(ENCRYPTED_PREFIX) + Base64Encode(encrypted);
}

std::string KeyEncryption::Decrypt(const std::string& ciphertext)
{
    if (ciphertext.empty())
        return "";
    
    // Check for prefix
    if (ciphertext.find(ENCRYPTED_PREFIX) != 0)
    {
        // Not encrypted, return as-is
        return ciphertext;
    }
    
    // Remove prefix and decode
    const std::string encoded = ciphertext.substr(strlen(ENCRYPTED_PREFIX));
    const std::string decoded = Base64Decode(encoded);
    
    // XOR with device key to decrypt
    const std::string deviceKey = GetDeviceKey();
    return XorCipher(decoded, deviceKey);
}

bool KeyEncryption::IsEncrypted(const std::string& value)
{
    return value.find(ENCRYPTED_PREFIX) == 0;
}

} // namespace util
