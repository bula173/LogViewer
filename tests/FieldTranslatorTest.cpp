/**
 * @file FieldTranslatorTest.cpp
 * @brief Specification-based unit tests for config::FieldTranslator.
 *
 * Tests are derived from the documented API contract in FieldTranslator.hpp.
 *
 * Covered specifications:
 *  - HasTranslation() returns false for unknown keys.
 *  - SetTranslation() registers an entry; HasTranslation() then returns true.
 *  - HasTranslation() is case-insensitive.
 *  - Translate() with a value_map conversion finds the mapped value and
 *    sets wasConverted = true; convertedValue contains the mapped word.
 *  - Translate() for an unknown key returns the original value with
 *    wasConverted = false.
 *  - Translate() for a known key with an unmapped value returns the original
 *    value and wasConverted = false.
 *  - RemoveTranslation() removes an entry; HasTranslation() then returns false.
 *  - GetAllTranslations() reflects all added entries.
 *  - SaveToFile() + LoadFromFile() round-trip preserves entries.
 *  - LoadFromFile() returns false for a missing file.
 */

#include <gtest/gtest.h>
#include "FieldTranslator.hpp"
#include "BuiltinConversionPlugins.hpp"

#include <filesystem>
#include <string>

namespace fs = std::filesystem;

// ============================================================================
// Test fixture — registers built-in conversion plugins once per suite
// ============================================================================

class FieldTranslatorTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        // The value_map plugin must be registered so that Translate() can
        // apply value-map conversions through the FieldConversionPluginRegistry.
        config::RegisterBuiltinConversionPlugins();
    }
};

// ============================================================================
// Helpers
// ============================================================================

/** Build a FieldDictionary entry with a value_map conversion. */
static config::FieldDictionary MakeValueMapEntry(
    const std::string& key,
    const std::map<std::string, std::string>& map)
{
    config::FieldDictionary d;
    d.key            = key;
    d.conversionType = "value_map";
    d.tooltipTemplate = "";
    d.valueMap       = map;
    return d;
}

/** RAII temp-file helper. */
struct TempFile
{
    fs::path path;
    explicit TempFile(const std::string& suffix)
        : path(fs::temp_directory_path() / ("ft_test_" + suffix + ".json"))
    {}
    ~TempFile() { fs::remove(path); }
};

// ============================================================================
// HasTranslation
// ============================================================================

/**
 * @brief Spec: HasTranslation() returns false for an unregistered key.
 */
TEST_F(FieldTranslatorTest, HasTranslationReturnsFalseForUnknownKey)
{
    config::FieldTranslator ft;
    EXPECT_FALSE(ft.HasTranslation("nonexistent"));
}

/**
 * @brief Spec: After SetTranslation(), HasTranslation() returns true for that key.
 */
TEST_F(FieldTranslatorTest, HasTranslationReturnsTrueAfterSet)
{
    config::FieldTranslator ft;
    ft.SetTranslation(MakeValueMapEntry("status", {{"0", "Off"}}));
    EXPECT_TRUE(ft.HasTranslation("status"));
}

/**
 * @brief Spec: HasTranslation() is case-insensitive.
 */
TEST_F(FieldTranslatorTest, HasTranslationIsCaseInsensitive)
{
    config::FieldTranslator ft;
    ft.SetTranslation(MakeValueMapEntry("STATUS", {{"0", "Off"}}));
    EXPECT_TRUE(ft.HasTranslation("status"));
    EXPECT_TRUE(ft.HasTranslation("STATUS"));
    EXPECT_TRUE(ft.HasTranslation("Status"));
}

// ============================================================================
// Translate
// ============================================================================

/**
 * @brief Spec: Translate() on a known key with a mapped value sets
 * wasConverted = true and includes the mapped word in convertedValue.
 */
TEST_F(FieldTranslatorTest, TranslateReturnsConvertedValueWhenMappingFound)
{
    config::FieldTranslator ft;
    ft.SetTranslation(MakeValueMapEntry("mode", {{"0", "Standby"}, {"1", "Active"}}));

    auto result = ft.Translate("mode", "1");
    EXPECT_TRUE(result.wasConverted);
    EXPECT_NE(result.convertedValue.find("Active"), std::string::npos);
}

/**
 * @brief Spec: Translate() on a known key with an unmapped value returns the
 * original and wasConverted = false.
 */
TEST_F(FieldTranslatorTest, TranslateReturnsOriginalWhenValueNotInMap)
{
    config::FieldTranslator ft;
    ft.SetTranslation(MakeValueMapEntry("mode", {{"0", "Standby"}}));

    auto result = ft.Translate("mode", "99");
    EXPECT_FALSE(result.wasConverted);
    EXPECT_EQ(result.convertedValue, "99");
}

/**
 * @brief Spec: Translate() for an unknown key returns the original value and
 * wasConverted = false.
 */
TEST_F(FieldTranslatorTest, TranslateForUnknownKeyReturnsOriginal)
{
    config::FieldTranslator ft;
    auto result = ft.Translate("unknown_field", "hello");
    EXPECT_FALSE(result.wasConverted);
    EXPECT_EQ(result.convertedValue, "hello");
}

// ============================================================================
// RemoveTranslation
// ============================================================================

/**
 * @brief Spec: After RemoveTranslation(), HasTranslation() returns false.
 */
TEST_F(FieldTranslatorTest, RemoveTranslationMakesHasTranslationReturnFalse)
{
    config::FieldTranslator ft;
    ft.SetTranslation(MakeValueMapEntry("key1", {{"0", "No"}}));
    ASSERT_TRUE(ft.HasTranslation("key1"));

    ft.RemoveTranslation("key1");
    EXPECT_FALSE(ft.HasTranslation("key1"));
}

/**
 * @brief Spec: RemoveTranslation() on a non-existent key does not crash.
 */
TEST_F(FieldTranslatorTest, RemoveTranslationNonExistentKeyDoesNotCrash)
{
    config::FieldTranslator ft;
    EXPECT_NO_THROW(ft.RemoveTranslation("ghost"));
}

// ============================================================================
// SetTranslation (update existing)
// ============================================================================

/**
 * @brief Spec: SetTranslation() with an existing key updates the entry.
 */
TEST_F(FieldTranslatorTest, SetTranslationUpdatesExistingEntry)
{
    config::FieldTranslator ft;
    ft.SetTranslation(MakeValueMapEntry("flag", {{"0", "Off"}}));
    ft.SetTranslation(MakeValueMapEntry("flag", {{"0", "Disabled"}}));

    auto result = ft.Translate("flag", "0");
    EXPECT_TRUE(result.wasConverted);
    EXPECT_NE(result.convertedValue.find("Disabled"), std::string::npos);
}

// ============================================================================
// GetAllTranslations
// ============================================================================

/**
 * @brief Spec: GetAllTranslations() reflects every entry added via SetTranslation().
 */
TEST_F(FieldTranslatorTest, GetAllTranslationsReflectsAddedEntries)
{
    config::FieldTranslator ft;
    ft.SetTranslation(MakeValueMapEntry("alpha", {}));
    ft.SetTranslation(MakeValueMapEntry("beta",  {}));

    const auto& all = ft.GetAllTranslations();
    EXPECT_EQ(all.count("alpha"), 1u);
    EXPECT_EQ(all.count("beta"),  1u);
}

/**
 * @brief Spec: A freshly constructed FieldTranslator has an empty dictionary.
 */
TEST_F(FieldTranslatorTest, GetAllTranslationsEmptyByDefault)
{
    config::FieldTranslator ft;
    EXPECT_TRUE(ft.GetAllTranslations().empty());
}

// ============================================================================
// SaveToFile / LoadFromFile
// ============================================================================

/**
 * @brief Spec: SaveToFile() + LoadFromFile() round-trips an entry; the loaded
 * translator can translate the same value.
 */
TEST_F(FieldTranslatorTest, SaveAndLoadRoundTripPreservesEntry)
{
    TempFile tmp("roundtrip");

    {
        config::FieldTranslator ft;
        ft.SetTranslation(MakeValueMapEntry("signal", {{"0", "Red"}, {"1", "Green"}}));
        ASSERT_TRUE(ft.SaveToFile(tmp.path.string()));
    }

    config::FieldTranslator ft2;
    ASSERT_TRUE(ft2.LoadFromFile(tmp.path.string()));
    EXPECT_TRUE(ft2.HasTranslation("signal"));

    auto result = ft2.Translate("signal", "1");
    EXPECT_TRUE(result.wasConverted);
    EXPECT_NE(result.convertedValue.find("Green"), std::string::npos);
}

/**
 * @brief Spec: LoadFromFile() returns false when the file does not exist.
 */
TEST_F(FieldTranslatorTest, LoadFromFileMissingFileReturnsFalse)
{
    config::FieldTranslator ft;
    EXPECT_FALSE(ft.LoadFromFile("/nonexistent/path/to/dictionary.json"));
}

/**
 * @brief Spec: Multiple entries survive a SaveToFile() + LoadFromFile() round-trip.
 */
TEST_F(FieldTranslatorTest, SaveAndLoadRoundTripMultipleEntries)
{
    TempFile tmp("multi");

    {
        config::FieldTranslator ft;
        ft.SetTranslation(MakeValueMapEntry("a", {{"1", "one"}}));
        ft.SetTranslation(MakeValueMapEntry("b", {{"2", "two"}}));
        ASSERT_TRUE(ft.SaveToFile(tmp.path.string()));
    }

    config::FieldTranslator ft2;
    ASSERT_TRUE(ft2.LoadFromFile(tmp.path.string()));
    EXPECT_TRUE(ft2.HasTranslation("a"));
    EXPECT_TRUE(ft2.HasTranslation("b"));
}

/**
 * @brief Spec: After SaveToFile() + LoadFromFile(), RemoveTranslation() makes
 * HasTranslation() return false.
 */
TEST_F(FieldTranslatorTest, RemoveAfterLoadWorks)
{
    TempFile tmp("remove");

    config::FieldTranslator ft;
    ft.SetTranslation(MakeValueMapEntry("x", {{"0", "Zero"}}));
    ASSERT_TRUE(ft.SaveToFile(tmp.path.string()));
    ASSERT_TRUE(ft.LoadFromFile(tmp.path.string()));
    ASSERT_TRUE(ft.HasTranslation("x"));

    ft.RemoveTranslation("x");
    EXPECT_FALSE(ft.HasTranslation("x"));
}
