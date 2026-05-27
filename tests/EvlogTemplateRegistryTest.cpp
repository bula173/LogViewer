#include <gtest/gtest.h>

#include "evlog/EvlogTemplateRegistry.hpp"
#include "evlog/EvlogTemplate.hpp"

#include <filesystem>
#include <fstream>
#include <string>

namespace parser
{
namespace test
{

// ---------------------------------------------------------------------------
// Helpers
// ---------------------------------------------------------------------------

static std::filesystem::path WriteTempFile(const std::string& name,
    const std::string& content)
{
    const auto path = std::filesystem::temp_directory_path() / name;
    std::ofstream f(path, std::ios::out | std::ios::trunc);
    f << content;
    return path;
}

// ---------------------------------------------------------------------------
// Basic loading and Find()
// ---------------------------------------------------------------------------

TEST(EvlogTemplateRegistryTest, EmptyRegistry_FindReturnsNull)
{
    EvlogTemplateRegistry reg;
    EXPECT_EQ(reg.Find(8, 1), nullptr);
    EXPECT_EQ(reg.Count(), 0u);
}

TEST(EvlogTemplateRegistryTest, Load_SingleTemplate_FoundByNumericFacility)
{
    const std::string tmpl =
        "facility 8\n"
        "event_type 1\n"
        "description \"test event\"\n";
    const auto path = WriteTempFile("reg_single.t", tmpl);

    EvlogTemplateRegistry reg;
    reg.LoadFromFile(path);

    EXPECT_EQ(reg.Count(), 1u);
    const EvlogTemplate* t = reg.Find(8, 1);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->facility, 8);
    EXPECT_EQ(t->eventType, 1u);
    EXPECT_EQ(t->description, "test event");
}

TEST(EvlogTemplateRegistryTest, Load_NonExistentFile_CountStaysZero)
{
    EvlogTemplateRegistry reg;
    reg.LoadFromFile("/tmp/this_file_does_not_exist_xyz.t");
    EXPECT_EQ(reg.Count(), 0u);
}

TEST(EvlogTemplateRegistryTest, Find_WrongEventType_ReturnsNull)
{
    const std::string tmpl =
        "facility 8\n"
        "event_type 1\n";
    const auto path = WriteTempFile("reg_wrong_et.t", tmpl);

    EvlogTemplateRegistry reg;
    reg.LoadFromFile(path);
    EXPECT_EQ(reg.Find(8, 99), nullptr);
}

TEST(EvlogTemplateRegistryTest, Find_WrongFacility_ReturnsNull)
{
    const std::string tmpl =
        "facility 8\n"
        "event_type 1\n";
    const auto path = WriteTempFile("reg_wrong_fac.t", tmpl);

    EvlogTemplateRegistry reg;
    reg.LoadFromFile(path);
    EXPECT_EQ(reg.Find(16, 1), nullptr);
}

// ---------------------------------------------------------------------------
// Named facility codes
// ---------------------------------------------------------------------------

TEST(EvlogTemplateRegistryTest, NamedFacility_LogUser_Resolves_To_8)
{
    const std::string tmpl =
        "facility LOG_USER\n"
        "event_type 5\n";
    const auto path = WriteTempFile("reg_named_fac.t", tmpl);

    EvlogTemplateRegistry reg;
    reg.LoadFromFile(path);
    ASSERT_NE(reg.Find(8, 5), nullptr);
}

TEST(EvlogTemplateRegistryTest, NamedFacility_LogLocal0_Resolves_To_128)
{
    const std::string tmpl =
        "facility LOG_LOCAL0\n"
        "event_type 42\n";
    const auto path = WriteTempFile("reg_local0.t", tmpl);

    EvlogTemplateRegistry reg;
    reg.LoadFromFile(path);
    ASSERT_NE(reg.Find(128, 42), nullptr);
}

TEST(EvlogTemplateRegistryTest, HexFacility_Parsed)
{
    const std::string tmpl =
        "facility 0x08\n"   // hex 8 == LOG_USER
        "event_type 3\n";
    const auto path = WriteTempFile("reg_hex_fac.t", tmpl);

    EvlogTemplateRegistry reg;
    reg.LoadFromFile(path);
    ASSERT_NE(reg.Find(8, 3), nullptr);
}

// ---------------------------------------------------------------------------
// Field declarations
// ---------------------------------------------------------------------------

TEST(EvlogTemplateRegistryTest, Fields_IntTypes_ParsedWithCorrectType)
{
    const std::string tmpl =
        "facility 8\n"
        "event_type 10\n"
        "attributes {\n"
        "  int uid;\n"
        "  uint pid;\n"
        "  short code;\n"
        "  ushort flags;\n"
        "}\n";
    const auto path = WriteTempFile("reg_inttypes.t", tmpl);

    EvlogTemplateRegistry reg;
    reg.LoadFromFile(path);
    const EvlogTemplate* t = reg.Find(8, 10);
    ASSERT_NE(t, nullptr);
    ASSERT_EQ(t->fields.size(), 4u);
    EXPECT_EQ(t->fields[0].name, "uid");
    EXPECT_EQ(t->fields[0].type, EvlogTemplateField::Type::Int32);
    EXPECT_EQ(t->fields[1].name, "pid");
    EXPECT_EQ(t->fields[1].type, EvlogTemplateField::Type::UInt32);
    EXPECT_EQ(t->fields[2].name, "code");
    EXPECT_EQ(t->fields[2].type, EvlogTemplateField::Type::Int16);
    EXPECT_EQ(t->fields[3].name, "flags");
    EXPECT_EQ(t->fields[3].type, EvlogTemplateField::Type::UInt16);
}

TEST(EvlogTemplateRegistryTest, Fields_64BitTypes_Parsed)
{
    const std::string tmpl =
        "facility 8\n"
        "event_type 20\n"
        "attributes {\n"
        "  int64_t timestamp;\n"
        "  uint64_t size;\n"
        "}\n";
    const auto path = WriteTempFile("reg_64bit.t", tmpl);

    EvlogTemplateRegistry reg;
    reg.LoadFromFile(path);
    const EvlogTemplate* t = reg.Find(8, 20);
    ASSERT_NE(t, nullptr);
    ASSERT_EQ(t->fields.size(), 2u);
    EXPECT_EQ(t->fields[0].type, EvlogTemplateField::Type::Int64);
    EXPECT_EQ(t->fields[1].type, EvlogTemplateField::Type::UInt64);
}

TEST(EvlogTemplateRegistryTest, Fields_FloatDouble_Parsed)
{
    const std::string tmpl =
        "facility 8\n"
        "event_type 30\n"
        "attributes {\n"
        "  float temperature;\n"
        "  double pressure;\n"
        "}\n";
    const auto path = WriteTempFile("reg_floats.t", tmpl);

    EvlogTemplateRegistry reg;
    reg.LoadFromFile(path);
    const EvlogTemplate* t = reg.Find(8, 30);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->fields[0].type, EvlogTemplateField::Type::Float);
    EXPECT_EQ(t->fields[1].type, EvlogTemplateField::Type::Double);
}

TEST(EvlogTemplateRegistryTest, Fields_FixedSizeCharArray_IsStringType)
{
    const std::string tmpl =
        "facility 8\n"
        "event_type 40\n"
        "attributes {\n"
        "  char hostname[64];\n"
        "}\n";
    const auto path = WriteTempFile("reg_chararray.t", tmpl);

    EvlogTemplateRegistry reg;
    reg.LoadFromFile(path);
    const EvlogTemplate* t = reg.Find(8, 40);
    ASSERT_NE(t, nullptr);
    ASSERT_EQ(t->fields.size(), 1u);
    EXPECT_EQ(t->fields[0].name, "hostname");
    EXPECT_EQ(t->fields[0].type, EvlogTemplateField::Type::String);
    EXPECT_EQ(t->fields[0].fixedSize, 64u);
}

TEST(EvlogTemplateRegistryTest, Fields_CString_VariableLength)
{
    const std::string tmpl =
        "facility 8\n"
        "event_type 50\n"
        "attributes {\n"
        "  string msg;\n"
        "}\n";
    const auto path = WriteTempFile("reg_cstring.t", tmpl);

    EvlogTemplateRegistry reg;
    reg.LoadFromFile(path);
    const EvlogTemplate* t = reg.Find(8, 50);
    ASSERT_NE(t, nullptr);
    ASSERT_EQ(t->fields.size(), 1u);
    EXPECT_EQ(t->fields[0].name, "msg");
    EXPECT_EQ(t->fields[0].type, EvlogTemplateField::Type::CString);
    EXPECT_EQ(t->fields[0].fixedSize, 0u);
}

// ---------------------------------------------------------------------------
// Format strings
// ---------------------------------------------------------------------------

TEST(EvlogTemplateRegistryTest, FormatString_Stored)
{
    const std::string tmpl =
        "facility 8\n"
        "event_type 60\n"
        "format \"uid=%uid% host=%hostname%\"\n"
        "attributes {\n"
        "  int uid;\n"
        "  char hostname[64];\n"
        "}\n";
    const auto path = WriteTempFile("reg_fmt.t", tmpl);

    EvlogTemplateRegistry reg;
    reg.LoadFromFile(path);
    const EvlogTemplate* t = reg.Find(8, 60);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->formatStr, "uid=%uid% host=%hostname%");
}

TEST(EvlogTemplateRegistryTest, FormatString_UnquotedStored)
{
    const std::string tmpl =
        "facility 8\n"
        "event_type 61\n"
        "format uid=%uid%\n";
    const auto path = WriteTempFile("reg_fmt_unquoted.t", tmpl);

    EvlogTemplateRegistry reg;
    reg.LoadFromFile(path);
    const EvlogTemplate* t = reg.Find(8, 61);
    ASSERT_NE(t, nullptr);
    EXPECT_EQ(t->formatStr, "uid=%uid%");
}

// ---------------------------------------------------------------------------
// Comments and blank lines
// ---------------------------------------------------------------------------

TEST(EvlogTemplateRegistryTest, Comments_SlashSlash_Stripped)
{
    const std::string tmpl =
        "// This is a comment\n"
        "facility 8   // inline comment\n"
        "event_type 70 // also inline\n";
    const auto path = WriteTempFile("reg_comment_slash.t", tmpl);

    EvlogTemplateRegistry reg;
    reg.LoadFromFile(path);
    ASSERT_NE(reg.Find(8, 70), nullptr);
}

TEST(EvlogTemplateRegistryTest, Comments_Hash_Stripped)
{
    const std::string tmpl =
        "# hash comment\n"
        "facility 8 # inline\n"
        "event_type 71\n";
    const auto path = WriteTempFile("reg_comment_hash.t", tmpl);

    EvlogTemplateRegistry reg;
    reg.LoadFromFile(path);
    ASSERT_NE(reg.Find(8, 71), nullptr);
}

// ---------------------------------------------------------------------------
// Multiple templates in one file (separated by ---)
// ---------------------------------------------------------------------------

TEST(EvlogTemplateRegistryTest, MultiTemplate_SeparatedByDashes_BothLoaded)
{
    const std::string tmpl =
        "facility 8\n"
        "event_type 100\n"
        "description \"first\"\n"
        "---\n"
        "facility 16\n"
        "event_type 200\n"
        "description \"second\"\n";
    const auto path = WriteTempFile("reg_multi.t", tmpl);

    EvlogTemplateRegistry reg;
    reg.LoadFromFile(path);
    EXPECT_EQ(reg.Count(), 2u);
    ASSERT_NE(reg.Find(8, 100), nullptr);
    ASSERT_NE(reg.Find(16, 200), nullptr);
    EXPECT_EQ(reg.Find(8, 100)->description, "first");
    EXPECT_EQ(reg.Find(16, 200)->description, "second");
}

TEST(EvlogTemplateRegistryTest, MultiTemplate_SameFacilityDifferentEventType)
{
    const std::string tmpl =
        "facility 8\n"
        "event_type 1\n"
        "---\n"
        "facility 8\n"
        "event_type 2\n";
    const auto path = WriteTempFile("reg_multi_same_fac.t", tmpl);

    EvlogTemplateRegistry reg;
    reg.LoadFromFile(path);
    EXPECT_EQ(reg.Count(), 2u);
    ASSERT_NE(reg.Find(8, 1), nullptr);
    ASSERT_NE(reg.Find(8, 2), nullptr);
}

// ---------------------------------------------------------------------------
// %keyword% form for facility
// ---------------------------------------------------------------------------

TEST(EvlogTemplateRegistryTest, PercentKeyword_FacilityAccepted)
{
    const std::string tmpl =
        "%facility% 8\n"
        "%event_type% 77\n";
    const auto path = WriteTempFile("reg_percent.t", tmpl);

    EvlogTemplateRegistry reg;
    reg.LoadFromFile(path);
    ASSERT_NE(reg.Find(8, 77), nullptr);
}

// ---------------------------------------------------------------------------
// LoadFromDirectory — only recognised extensions loaded
// ---------------------------------------------------------------------------

TEST(EvlogTemplateRegistryTest, LoadFromDirectory_LoadsTAndTmplFiles)
{
    const auto dir = std::filesystem::temp_directory_path() / "evlog_tmpl_dir_test";
    std::filesystem::create_directories(dir);

    // .t file — should be loaded
    {
        std::ofstream f(dir / "a.t");
        f << "facility 8\nevent_type 1\n";
    }
    // .tmpl file — should be loaded
    {
        std::ofstream f(dir / "b.tmpl");
        f << "facility 16\nevent_type 2\n";
    }
    // .template file — should be loaded
    {
        std::ofstream f(dir / "c.template");
        f << "facility 24\nevent_type 3\n";
    }
    // .txt file — should NOT be loaded
    {
        std::ofstream f(dir / "d.txt");
        f << "facility 32\nevent_type 4\n";
    }

    EvlogTemplateRegistry reg;
    reg.LoadFromDirectory(dir);

    EXPECT_EQ(reg.Count(), 3u);
    EXPECT_NE(reg.Find(8, 1), nullptr);
    EXPECT_NE(reg.Find(16, 2), nullptr);
    EXPECT_NE(reg.Find(24, 3), nullptr);
    EXPECT_EQ(reg.Find(32, 4), nullptr);  // .txt must be ignored

    std::filesystem::remove_all(dir);
}

TEST(EvlogTemplateRegistryTest, LoadFromDirectory_NonExistent_NoCount)
{
    EvlogTemplateRegistry reg;
    reg.LoadFromDirectory("/tmp/this_directory_does_not_exist_xyz");
    EXPECT_EQ(reg.Count(), 0u);
}

// ---------------------------------------------------------------------------
// LoadFromFile multiple calls accumulate templates
// ---------------------------------------------------------------------------

TEST(EvlogTemplateRegistryTest, MultipleLoadFromFile_Accumulate)
{
    const auto p1 = WriteTempFile("reg_acc1.t", "facility 8\nevent_type 1\n");
    const auto p2 = WriteTempFile("reg_acc2.t", "facility 16\nevent_type 2\n");

    EvlogTemplateRegistry reg;
    reg.LoadFromFile(p1);
    reg.LoadFromFile(p2);

    EXPECT_EQ(reg.Count(), 2u);
    EXPECT_NE(reg.Find(8, 1), nullptr);
    EXPECT_NE(reg.Find(16, 2), nullptr);
}

} // namespace test
} // namespace parser
