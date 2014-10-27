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
TEST(StrUtilTest,StringStrip)
{
   EXPECT_EQ("", APT::String::Strip(""));
   EXPECT_EQ("foobar", APT::String::Strip("foobar"));
   EXPECT_EQ("foo bar", APT::String::Strip("foo bar"));

   EXPECT_EQ("", APT::String::Strip("  "));
   EXPECT_EQ("", APT::String::Strip(" \r\n   \t "));

   EXPECT_EQ("foo bar", APT::String::Strip("foo bar "));
   EXPECT_EQ("foo bar", APT::String::Strip("foo bar \r\n \t "));
   EXPECT_EQ("foo bar", APT::String::Strip("\r\n \t foo bar"));
   EXPECT_EQ("bar foo", APT::String::Strip("\r\n \t bar foo \r\n \t "));
   EXPECT_EQ("bar \t\r\n foo", APT::String::Strip("\r\n \t bar \t\r\n foo \r\n \t "));
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
TEST(StrUtilTest,SubstVar)
{
   EXPECT_EQ("", SubstVar("", "fails", "passes"));
   EXPECT_EQ("test ", SubstVar("test fails", "fails", ""));
   EXPECT_EQ("test passes", SubstVar("test passes", "", "fails"));

   EXPECT_EQ("test passes", SubstVar("test passes", "fails", "passes"));
   EXPECT_EQ("test passes", SubstVar("test fails", "fails", "passes"));

   EXPECT_EQ("starts with", SubstVar("beginnt with", "beginnt", "starts"));
   EXPECT_EQ("beginnt with", SubstVar("starts with", "starts", "beginnt"));
   EXPECT_EQ("is in middle", SubstVar("is in der middle", "in der", "in"));
   EXPECT_EQ("is in der middle", SubstVar("is in middle", "in", "in der"));
   EXPECT_EQ("does end", SubstVar("does enden", "enden", "end"));
   EXPECT_EQ("does enden", SubstVar("does end", "end", "enden"));

   EXPECT_EQ("abc", SubstVar("abc", "d", "a"));
   EXPECT_EQ("abc", SubstVar("abd", "d", "c"));
   EXPECT_EQ("abc", SubstVar("adc", "d", "b"));
   EXPECT_EQ("abc", SubstVar("dbc", "d", "a"));

   EXPECT_EQ("b", SubstVar("b", "aa", "a"));
   EXPECT_EQ("bb", SubstVar("bb", "aa", "a"));
   EXPECT_EQ("bbb", SubstVar("bbb", "aa", "a"));

   EXPECT_EQ("aa", SubstVar("aaaa", "aa", "a"));
   EXPECT_EQ("aaaa", SubstVar("aa", "a", "aa"));
   EXPECT_EQ("aaaa", SubstVar("aaaa", "a", "a"));
   EXPECT_EQ("a a a a ", SubstVar("aaaa", "a", "a "));

   EXPECT_EQ(" bb bb bb bb ", SubstVar(" a a a a ", "a", "bb"));
   EXPECT_EQ(" bb bb bb bb ", SubstVar(" aaa aaa aaa aaa ", "aaa", "bb"));
   EXPECT_EQ(" bb a bb a bb a bb ", SubstVar(" aaa a aaa a aaa a aaa ", "aaa", "bb"));

}
