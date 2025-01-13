#include <gtest/gtest.h>
#include <sstream>
#include "src/application/parser/wxStdInputStreamAdapter.hpp"

TEST(wxStdInputStreamAdapterTest, ReadPercentageCalculation)
{
  std::istringstream input("This is a test string.");
  wxStdInputStreamAdapter adapter(input);

  // Initially, the read percentage should be 0
  EXPECT_DOUBLE_EQ(adapter.GetReadPercentage(), 0.0);

  char buffer[5];
  adapter.OnSysRead(buffer, 5);

  // After reading 5 bytes, the read percentage should be updated
  EXPECT_DOUBLE_EQ(adapter.GetReadPercentage(), 5.0 / adapter.GetTotalSize() * 100.0);
}

TEST(wxStdInputStreamAdapterTest, CanRead)
{
  std::istringstream input("This is a test string.");
  wxStdInputStreamAdapter adapter(input);

  // Initially, the stream should be readable
  EXPECT_TRUE(adapter.CanRead());

  char buffer[50];
  adapter.OnSysRead(buffer, 50);

  EXPECT_FALSE(adapter.CanRead());
}

TEST(wxStdInputStreamAdapterTest, Eof)
{
  std::istringstream input("This is a test string.");
  wxStdInputStreamAdapter adapter(input);

  char buffer[50];
  adapter.OnSysRead(buffer, 50);

  // After reading all data, the stream should be at EOF
  EXPECT_TRUE(adapter.Eof());
}

TEST(wxStdInputStreamAdapterTest, ReadData)
{
  std::istringstream input("This is a test string.");
  wxStdInputStreamAdapter adapter(input);

  char buffer[22];
  size_t bytesRead = adapter.OnSysRead(buffer, 22);

  // The number of bytes read should be equal to the length of the input string
  EXPECT_EQ(bytesRead, 22);

  // The content read should match the input string
  EXPECT_EQ(std::string(buffer, bytesRead), "This is a test string.") << "Read:" << ::testing::PrintToString(buffer);
}

TEST(wxStdInputStreamAdapterTest, GetTotalSize)
{
  std::istringstream input("This is a test string.");
  wxStdInputStreamAdapter adapter(input);

  // The number of total bytes should match the length of the input string + EOF
  EXPECT_EQ(adapter.GetTotalSize(), 22);
}