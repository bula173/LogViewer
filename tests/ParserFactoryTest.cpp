/**
 * @file ParserFactoryTest.cpp
 * @brief Specification-based unit tests for parser::ParserFactory.
 *
 * Tests are derived from the documented API in ParserFactory.hpp.
 *
 * Covered specifications:
 *  - CreateFromFile() returns an Ok result for a known extension (.xml).
 *  - CreateFromFile() returns an Ok result for a known extension (.csv).
 *  - CreateFromFile() returns a non-null parser when Ok.
 *  - Create(ParserType::XML) returns an Ok result with a non-null parser.
 *  - IsRegistered() returns true for extensions that have been registered.
 *  - IsRegistered() returns false for unregistered extensions.
 *  - Register() succeeds for a new extension (returns std::nullopt).
 *  - Register() makes IsRegistered() return true for that extension.
 *  - CreateFromFile() uses a custom-registered parser for the custom extension.
 *  - GetSupportedExtensions() includes at least the built-in .xml extension.
 */

#include <gtest/gtest.h>
#include "ParserFactory.hpp"
#include "IDataParser.hpp"

#include <filesystem>
#include <memory>
#include <string>
#include <vector>
#include <algorithm>

namespace fs = std::filesystem;

// ============================================================================
// CreateFromFile — built-in extensions
// ============================================================================

/**
 * @brief Spec: CreateFromFile() with an .xml path returns an Ok result.
 */
TEST(ParserFactoryTest, CreateFromFileXmlReturnsOk)
{
    auto result = parser::ParserFactory::CreateFromFile("log.xml");
    EXPECT_TRUE(result.isOk());
}

/**
 * @brief Spec: CreateFromFile() for a .csv path returns an Ok result.
 */
TEST(ParserFactoryTest, CreateFromFileCsvReturnsOk)
{
    auto result = parser::ParserFactory::CreateFromFile("data.csv");
    EXPECT_TRUE(result.isOk());
}

/**
 * @brief Spec: When CreateFromFile() succeeds the parser pointer is non-null.
 */
TEST(ParserFactoryTest, CreateFromFileReturnsNonNullParser)
{
    auto result = parser::ParserFactory::CreateFromFile("log.xml");
    ASSERT_TRUE(result.isOk());
    EXPECT_NE(result.unwrap(), nullptr);
}

// ============================================================================
// Create(ParserType)
// ============================================================================

/**
 * @brief Spec: Create(ParserType::XML) returns an Ok result.
 */
TEST(ParserFactoryTest, CreateXmlTypeReturnsOk)
{
    auto result = parser::ParserFactory::Create(parser::ParserType::XML);
    EXPECT_TRUE(result.isOk());
}

/**
 * @brief Spec: Create(ParserType::XML) returns a non-null parser.
 */
TEST(ParserFactoryTest, CreateXmlTypeReturnsNonNullParser)
{
    auto result = parser::ParserFactory::Create(parser::ParserType::XML);
    ASSERT_TRUE(result.isOk());
    EXPECT_NE(result.unwrap(), nullptr);
}

/**
 * @brief Spec: Create(ParserType::CSV) returns an Ok result.
 */
TEST(ParserFactoryTest, CreateCsvTypeReturnsOk)
{
    auto result = parser::ParserFactory::Create(parser::ParserType::CSV);
    EXPECT_TRUE(result.isOk());
}

// ============================================================================
// IsRegistered
// ============================================================================

/**
 * @brief Spec: IsRegistered() returns true for ".xml" which is always built-in.
 */
TEST(ParserFactoryTest, IsRegisteredXmlExtension)
{
    EXPECT_TRUE(parser::ParserFactory::IsRegistered(".xml"));
}

/**
 * @brief Spec: IsRegistered() returns false for an extension that has not been
 * registered.
 */
TEST(ParserFactoryTest, IsRegisteredReturnsFalseForUnknown)
{
    EXPECT_FALSE(parser::ParserFactory::IsRegistered(".does_not_exist_xyz"));
}

// ============================================================================
// Register + custom parser
// ============================================================================

/**
 * @brief Minimal parser stub used to verify custom registration.
 */
class NullParser : public parser::IDataParser
{
public:
    void ParseData(const std::filesystem::path&) override {}
    void ParseData(std::istream&) override {}
    uint32_t GetCurrentProgress() const override { return 0; }
    uint32_t GetTotalProgress() const override { return 0; }
};

/**
 * @brief Spec: Register() for a new extension returns std::nullopt (no error).
 */
TEST(ParserFactoryTest, RegisterNewExtensionReturnsNoError)
{
    auto err = parser::ParserFactory::Register(".testfmt1",
        []() -> std::unique_ptr<parser::IDataParser> {
            return std::make_unique<NullParser>();
        });
    EXPECT_FALSE(err.has_value());
}

/**
 * @brief Spec: After Register(), IsRegistered() returns true for that extension.
 */
TEST(ParserFactoryTest, IsRegisteredTrueAfterRegister)
{
    parser::ParserFactory::Register(".testfmt2",
        []() -> std::unique_ptr<parser::IDataParser> {
            return std::make_unique<NullParser>();
        });
    EXPECT_TRUE(parser::ParserFactory::IsRegistered(".testfmt2"));
}

/**
 * @brief Spec: CreateFromFile() uses the custom-registered creator for the
 * custom extension — the returned parser is non-null.
 */
TEST(ParserFactoryTest, CreateFromFileUsesCustomParser)
{
    parser::ParserFactory::Register(".testfmt3",
        []() -> std::unique_ptr<parser::IDataParser> {
            return std::make_unique<NullParser>();
        });

    auto result = parser::ParserFactory::CreateFromFile("something.testfmt3");
    ASSERT_TRUE(result.isOk());
    EXPECT_NE(result.unwrap(), nullptr);
}

// ============================================================================
// GetSupportedExtensions
// ============================================================================

/**
 * @brief Spec: GetSupportedExtensions() returns a non-empty list containing at
 * least the built-in ".xml" extension.
 */
TEST(ParserFactoryTest, GetSupportedExtensionsContainsXml)
{
    auto exts = parser::ParserFactory::GetSupportedExtensions();
    EXPECT_FALSE(exts.empty());
    auto it = std::find(exts.begin(), exts.end(), ".xml");
    EXPECT_NE(it, exts.end());
}
