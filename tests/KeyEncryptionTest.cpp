/**
 * @file KeyEncryptionTest.cpp
 * @brief Specification-based unit tests for util::KeyEncryption.
 *
 * Tests are derived from the documented API in KeyEncryption.hpp.
 *
 * Covered specifications:
 *  - Encrypt() returns a non-empty string for a non-empty plaintext.
 *  - Decrypt(Encrypt(plaintext)) round-trips back to the original plaintext.
 *  - Encrypt() of different plaintexts produces different ciphertext strings.
 *  - Encrypt() of an empty string returns a value that Decrypt() can recover
 *    (or at least does not crash).
 *  - IsEncrypted() returns true for a string that was produced by Encrypt().
 *  - IsEncrypted() returns false for a plain text string.
 *  - IsEncrypted() returns false for an empty string.
 */

#include <gtest/gtest.h>
#include "KeyEncryption.hpp"

#include <string>

// ============================================================================
// Encrypt
// ============================================================================

/**
 * @brief Spec: Encrypt() of a non-empty plaintext returns a non-empty string.
 */
TEST(KeyEncryptionTest, EncryptNonEmptyReturnsNonEmpty)
{
    std::string cipher = util::KeyEncryption::Encrypt("my-api-key-1234");
    EXPECT_FALSE(cipher.empty());
}

/**
 * @brief Spec: Encrypt() does not return the original plaintext (it is
 * obfuscated/transformed).
 */
TEST(KeyEncryptionTest, EncryptedValueDiffersFromPlaintext)
{
    const std::string plaintext = "sk-supersecret";
    EXPECT_NE(util::KeyEncryption::Encrypt(plaintext), plaintext);
}

/**
 * @brief Spec: Two different plaintexts produce different ciphertext values.
 */
TEST(KeyEncryptionTest, DifferentPlaintextsProduceDifferentCiphertexts)
{
    std::string c1 = util::KeyEncryption::Encrypt("keyA");
    std::string c2 = util::KeyEncryption::Encrypt("keyB");
    EXPECT_NE(c1, c2);
}

// ============================================================================
// Decrypt
// ============================================================================

/**
 * @brief Spec: Decrypt(Encrypt(plaintext)) returns the original plaintext.
 */
TEST(KeyEncryptionTest, DecryptRoundTripsToOriginal)
{
    const std::string original = "my-api-key-abc123";
    std::string cipher  = util::KeyEncryption::Encrypt(original);
    std::string decoded = util::KeyEncryption::Decrypt(cipher);
    EXPECT_EQ(decoded, original);
}

/**
 * @brief Spec: Decrypt() round-trips an empty string without crashing.
 */
TEST(KeyEncryptionTest, DecryptRoundTripsEmptyString)
{
    std::string cipher  = util::KeyEncryption::Encrypt("");
    std::string decoded = util::KeyEncryption::Decrypt(cipher);
    EXPECT_EQ(decoded, "");
}

/**
 * @brief Spec: Decrypt(Encrypt(x)) preserves all printable ASCII characters.
 */
TEST(KeyEncryptionTest, DecryptPreservesAllPrintableAscii)
{
    const std::string plaintext = "abcdefghijklmnopqrstuvwxyz0123456789!@#$%-_";
    std::string cipher  = util::KeyEncryption::Encrypt(plaintext);
    EXPECT_EQ(util::KeyEncryption::Decrypt(cipher), plaintext);
}

// ============================================================================
// IsEncrypted
// ============================================================================

/**
 * @brief Spec: IsEncrypted() returns true for a value produced by Encrypt().
 */
TEST(KeyEncryptionTest, IsEncryptedReturnsTrueForEncryptedValue)
{
    std::string cipher = util::KeyEncryption::Encrypt("some-key");
    EXPECT_TRUE(util::KeyEncryption::IsEncrypted(cipher));
}

/**
 * @brief Spec: IsEncrypted() returns false for a plain ASCII string that was
 * not produced by Encrypt().
 */
TEST(KeyEncryptionTest, IsEncryptedReturnsFalseForPlainText)
{
    EXPECT_FALSE(util::KeyEncryption::IsEncrypted("my-plain-api-key"));
}

/**
 * @brief Spec: IsEncrypted() returns false for an empty string.
 */
TEST(KeyEncryptionTest, IsEncryptedReturnsFalseForEmptyString)
{
    EXPECT_FALSE(util::KeyEncryption::IsEncrypted(""));
}
