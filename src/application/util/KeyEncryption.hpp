#pragma once

#include <string>

namespace util
{

/**
 * @brief Simple API key encryption/decryption utility
 * 
 * Provides basic obfuscation for API keys stored in config files.
 * Uses XOR cipher with device-specific key generation.
 * Not military-grade encryption, but prevents casual reading of keys.
 */
class KeyEncryption
{
public:
    /**
     * @brief Encrypt an API key
     * @param plaintext The plain API key
     * @return Base64-encoded encrypted key
     */
    static std::string Encrypt(const std::string& plaintext);

    /**
     * @brief Decrypt an API key
     * @param ciphertext Base64-encoded encrypted key
     * @return Decrypted plain API key
     */
    static std::string Decrypt(const std::string& ciphertext);

    /**
     * @brief Check if a string appears to be encrypted
     * @param value String to check
     * @return True if the string looks like an encrypted value
     */
    static bool IsEncrypted(const std::string& value);

private:
    // Implementation helpers are internal to KeyEncryption.cpp
};

} // namespace util
