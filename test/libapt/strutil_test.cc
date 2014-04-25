#include <config.h>
#include <apt-pkg/strutl.h>
#include <string>
#include <vector>

#include <gtest/gtest.h>

TEST(StrUtilTest,DeEscapeString)
{
   // nothing special
   EXPECT_EQ("", DeEscapeString(""));
   EXPECT_EQ("foobar", DeEscapeString("foobar"));
   // hex and octal
   EXPECT_EQ("foo bar\nbaz", DeEscapeString("foo\\040bar\\x0abaz"));
   EXPECT_EQ("foo ", DeEscapeString("foo\\040"));
   EXPECT_EQ("\nbaz", DeEscapeString("\\x0abaz"));
   EXPECT_EQ("/media/Ubuntu 11.04 amd64", DeEscapeString("/media/Ubuntu\\04011.04\\040amd64"));
   // double slashes
   EXPECT_EQ("foo\\ x", DeEscapeString("foo\\\\ x"));
   EXPECT_EQ("\\foo\\", DeEscapeString("\\\\foo\\\\"));
}
TEST(StrUtilTest,StringSplitBasic)
{
   std::vector<std::string> result = StringSplit("", "");
   EXPECT_EQ(result.size(), 0);

   result = StringSplit("abc", "");
   EXPECT_EQ(result.size(), 0);

   result = StringSplit("", "abc");
   EXPECT_EQ(result.size(), 1);

   result = StringSplit("abc", "b");
   ASSERT_EQ(result.size(), 2);
   EXPECT_EQ(result[0], "a");
   EXPECT_EQ(result[1], "c");

   result = StringSplit("abc", "abc");
   ASSERT_EQ(result.size(), 2);
   EXPECT_EQ(result[0], "");
   EXPECT_EQ(result[1], "");
}
TEST(StrUtilTest,StringSplitDpkgStatus)
{
   std::string const input = "status: libnet1:amd64: unpacked";
   std::vector<std::string> result = StringSplit(input, "xxx");
   ASSERT_EQ(result.size(), 1);
   EXPECT_EQ(result[0], input);

   result = StringSplit(input, "");
   EXPECT_EQ(result.size(), 0);

   result = StringSplit(input, ": ");
   ASSERT_EQ(result.size(), 3);
   EXPECT_EQ(result[0], "status");
   EXPECT_EQ(result[1], "libnet1:amd64");
   EXPECT_EQ(result[2], "unpacked");

   result = StringSplit("x:y:z", ":", 2);
   ASSERT_EQ(result.size(), 2);
   EXPECT_EQ(result[0], "x");
   EXPECT_EQ(result[1], "y:z");
}
TEST(StrUtilTest,EndsWith)
{
   using APT::String::Endswith;
   EXPECT_TRUE(Endswith("abcd", "d"));
   EXPECT_TRUE(Endswith("abcd", "cd"));
   EXPECT_TRUE(Endswith("abcd", "abcd"));
   EXPECT_FALSE(Endswith("abcd", "x"));
   EXPECT_FALSE(Endswith("abcd", "abcndefg"));
}
