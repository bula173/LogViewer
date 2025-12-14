#include "util/KeyEncryption.hpp"
#include "util/Logger.hpp"
#include <array>
#include <sstream>
#include <iomanip>
#include <algorithm>

#ifdef __APPLE__
#include <sys/sysctl.h>
#elif defined(_WIN32)
#include <windows.h>
#include <intrin.h>
#else
#include <fstream>
#include <unistd.h>
#endif

namespace util
{

// Prefix to identify encrypted values
static constexpr const char* ENCRYPTED_PREFIX = "ENC:";

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
    
    for (unsigned char c : input)
    {
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
