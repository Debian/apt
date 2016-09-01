#include <config.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/fileutl.h>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "file-helpers.h"

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
TEST(StrUtilTest,StartsWith)
{
   using APT::String::Startswith;
   EXPECT_TRUE(Startswith("abcd", "a"));
   EXPECT_TRUE(Startswith("abcd", "ab"));
   EXPECT_TRUE(Startswith("abcd", "abcd"));
   EXPECT_FALSE(Startswith("abcd", "x"));
   EXPECT_FALSE(Startswith("abcd", "abcndefg"));
}
TEST(StrUtilTest,TimeToStr)
{
   EXPECT_EQ("0s", TimeToStr(0));
   EXPECT_EQ("42s", TimeToStr(42));
   EXPECT_EQ("9min 21s", TimeToStr((9*60) + 21));
   EXPECT_EQ("20min 42s", TimeToStr((20*60) + 42));
   EXPECT_EQ("10h 42min 21s", TimeToStr((10*3600) + (42*60) + 21));
   EXPECT_EQ("10h 42min 21s", TimeToStr((10*3600) + (42*60) + 21));
   EXPECT_EQ("1988d 3h 29min 7s", TimeToStr((1988*86400) + (3*3600) + (29*60) + 7));

   EXPECT_EQ("59s", TimeToStr(59));
   EXPECT_EQ("60s", TimeToStr(60));
   EXPECT_EQ("1min 1s", TimeToStr(61));
   EXPECT_EQ("59min 59s", TimeToStr(3599));
   EXPECT_EQ("60min 0s", TimeToStr(3600));
   EXPECT_EQ("1h 0min 1s", TimeToStr(3601));
   EXPECT_EQ("1h 1min 0s", TimeToStr(3660));
   EXPECT_EQ("23h 59min 59s", TimeToStr(86399));
   EXPECT_EQ("24h 0min 0s", TimeToStr(86400));
   EXPECT_EQ("1d 0h 0min 1s", TimeToStr(86401));
   EXPECT_EQ("1d 0h 1min 0s", TimeToStr(86460));
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
TEST(StrUtilTest,Base64Encode)
{
   EXPECT_EQ("QWxhZGRpbjpvcGVuIHNlc2FtZQ==", Base64Encode("Aladdin:open sesame"));
   EXPECT_EQ("cGxlYXN1cmUu", Base64Encode("pleasure."));
   EXPECT_EQ("bGVhc3VyZS4=", Base64Encode("leasure."));
   EXPECT_EQ("ZWFzdXJlLg==", Base64Encode("easure."));
   EXPECT_EQ("YXN1cmUu", Base64Encode("asure."));
   EXPECT_EQ("c3VyZS4=", Base64Encode("sure."));
   EXPECT_EQ("dXJlLg==", Base64Encode("ure."));
   EXPECT_EQ("cmUu", Base64Encode("re."));
   EXPECT_EQ("ZS4=", Base64Encode("e."));
   EXPECT_EQ("Lg==", Base64Encode("."));
   EXPECT_EQ("", Base64Encode(""));
}
static void ReadMessagesTestWithNewLine(char const * const nl, char const * const ab)
{
   SCOPED_TRACE(SubstVar(SubstVar(nl, "\n", "n"), "\r", "r") + " # " + ab);
   FileFd fd;
   std::string pkgA = "Package: pkgA\n"
      "Version: 1\n"
      "Size: 100\n"
      "Description: aaa\n"
      " aaa";
   std::string pkgB = "Package: pkgB\n"
      "Version: 1\n"
      "Flag: no\n"
      "Description: bbb";
   std::string pkgC = "Package: pkgC\n"
      "Version: 2\n"
      "Flag: yes\n"
      "Description:\n"
      " ccc";

   createTemporaryFile("readmessage", fd, NULL, (pkgA + nl + pkgB + nl + pkgC + nl).c_str());
   std::vector<std::string> list;
   EXPECT_TRUE(ReadMessages(fd.Fd(), list));
   EXPECT_EQ(3, list.size());
   EXPECT_EQ(pkgA, list[0]);
   EXPECT_EQ(pkgB, list[1]);
   EXPECT_EQ(pkgC, list[2]);

   size_t const msgsize = 63990;
   createTemporaryFile("readmessage", fd, NULL, NULL);
   for (size_t j = 0; j < msgsize; ++j)
      fd.Write(ab, strlen(ab));
   for (size_t i = 0; i < 21; ++i)
   {
      std::string msg;
      strprintf(msg, "msgsize=%zu  i=%zu", msgsize, i);
      SCOPED_TRACE(msg);
      fd.Seek((msgsize + (i - 1)) * strlen(ab));
      fd.Write(ab, strlen(ab));
      fd.Write(nl, strlen(nl));
      fd.Seek(0);
      list.clear();
      EXPECT_TRUE(ReadMessages(fd.Fd(), list));
      EXPECT_EQ(1, list.size());
      EXPECT_EQ((msgsize + i) * strlen(ab), list[0].length());
      EXPECT_EQ(std::string::npos, list[0].find_first_not_of(ab));
   }

   list.clear();
   fd.Write(pkgA.c_str(), pkgA.length());
   fd.Write(nl, strlen(nl));
   fd.Seek(0);
   EXPECT_TRUE(ReadMessages(fd.Fd(), list));
   EXPECT_EQ(2, list.size());
   EXPECT_EQ((msgsize + 20) * strlen(ab), list[0].length());
   EXPECT_EQ(std::string::npos, list[0].find_first_not_of(ab));
   EXPECT_EQ(pkgA, list[1]);


   fd.Close();
}
TEST(StrUtilTest,ReadMessages)
{
   ReadMessagesTestWithNewLine("\n\n", "a");
   ReadMessagesTestWithNewLine("\r\n\r\n", "a");
   ReadMessagesTestWithNewLine("\n\n", "ab");
   ReadMessagesTestWithNewLine("\r\n\r\n", "ab");
}
TEST(StrUtilTest,QuoteString)
{
   EXPECT_EQ("", QuoteString("", ""));
   EXPECT_EQ("K%c3%b6ln", QuoteString("Köln", ""));
   EXPECT_EQ("Köln", DeQuoteString(QuoteString("Köln", "")));
   EXPECT_EQ("Köln", DeQuoteString(DeQuoteString(QuoteString(QuoteString("Köln", ""), ""))));
   EXPECT_EQ("~-_$#|u%c3%a4%c3%b6%c5%a6%e2%84%a2%e2%85%9e%c2%b1%c3%86%e1%ba%9e%c2%aa%c3%9f", QuoteString("~-_$#|uäöŦ™⅞±Æẞªß", ""));
   EXPECT_EQ("~-_$#|uäöŦ™⅞±Æẞªß", DeQuoteString(QuoteString("~-_$#|uäöŦ™⅞±Æẞªß", "")));
   EXPECT_EQ("%45ltvill%65%2d%45rbach", QuoteString("Eltville-Erbach", "E-Ae"));
   EXPECT_EQ("Eltville-Erbach", DeQuoteString(QuoteString("Eltville-Erbach", "")));
}

TEST(StrUtilTest,RFC1123StrToTime)
{
   {
      time_t t;
      EXPECT_TRUE(RFC1123StrToTime("Sun, 06 Nov 1994 08:49:37 GMT", t));
      EXPECT_EQ(784111777, t);
   } {
      time_t t;
      EXPECT_TRUE(RFC1123StrToTime("Sun, 6 Nov 1994 08:49:37 UTC", t));
      EXPECT_EQ(784111777, t);
   } {
      time_t t;
      EXPECT_TRUE(RFC1123StrToTime("Sun,  6 Nov 1994 08:49:37 UTC", t));
      EXPECT_EQ(784111777, t);
   } {
      time_t t;
      EXPECT_TRUE(RFC1123StrToTime("Sun, 06 Nov 1994  8:49:37 UTC", t));
      EXPECT_EQ(784111777, t);
   } {
      time_t t;
      EXPECT_TRUE(RFC1123StrToTime("Sun, 06 Nov 1994 08:49:37 UTC", t));
      EXPECT_EQ(784111777, t);
   } {
      time_t t;
      EXPECT_TRUE(RFC1123StrToTime("Sun, 06 Nov 1994 08:49:37 -0000", t));
      EXPECT_EQ(784111777, t);
   } {
      time_t t;
      EXPECT_TRUE(RFC1123StrToTime("Sun, 06 Nov 1994 08:49:37 +0000", t));
      EXPECT_EQ(784111777, t);
   } {
      time_t t;
      EXPECT_TRUE(RFC1123StrToTime("Sunday, 06-Nov-94 08:49:37 GMT", t));
      EXPECT_EQ(784111777, t);
   } {
      time_t t;
      EXPECT_TRUE(RFC1123StrToTime("Sunday,  6-Nov-94 08:49:37 GMT", t));
      EXPECT_EQ(784111777, t);
   } {
      time_t t;
      EXPECT_TRUE(RFC1123StrToTime("Sunday, 06-Nov-94 8:49:37 GMT", t));
      EXPECT_EQ(784111777, t);
   } {
      time_t t;
      EXPECT_TRUE(RFC1123StrToTime("Sun Nov  6 08:49:37 1994", t));
      EXPECT_EQ(784111777, t);
   } {
      time_t t;
      EXPECT_TRUE(RFC1123StrToTime("Sun Nov 06 08:49:37 1994", t));
      EXPECT_EQ(784111777, t);
   } {
      time_t t;
      EXPECT_TRUE(RFC1123StrToTime("Sun Nov  6  8:49:37 1994", t));
      EXPECT_EQ(784111777, t);
   }
   time_t t;
   EXPECT_FALSE(RFC1123StrToTime("So, 06 Nov 1994 08:49:37 UTC", t));
   EXPECT_FALSE(RFC1123StrToTime(", 06 Nov 1994 08:49:37 UTC", t));
   EXPECT_FALSE(RFC1123StrToTime("Son, 06 Nov 1994 08:49:37 UTC", t));
   EXPECT_FALSE(RFC1123StrToTime("Sun: 06 Nov 1994 08:49:37 UTC", t));
   EXPECT_FALSE(RFC1123StrToTime("Sun, 06 Nov 1994 08:49:37", t));
   EXPECT_FALSE(RFC1123StrToTime("Sun, 06 Nov 1994 08:49:37 GMT+1", t));
   EXPECT_FALSE(RFC1123StrToTime("Sun, 06 Nov 1994 GMT", t));
   EXPECT_FALSE(RFC1123StrToTime("Sunday, 06 Nov 1994 GMT", t));
   EXPECT_FALSE(RFC1123StrToTime("Sonntag, 06 Nov 1994 08:49:37 GMT", t));
   EXPECT_FALSE(RFC1123StrToTime("domingo Nov 6 08:49:37 1994", t));
   EXPECT_FALSE(RFC1123StrToTime("Sunday: 06-Nov-94 08:49:37 GMT", t));
   EXPECT_FALSE(RFC1123StrToTime("Sunday, 06-Nov-94 08:49:37 GMT+1", t));
   EXPECT_FALSE(RFC1123StrToTime("Sunday, 06-Nov-94 08:49:37 EDT", t));
   EXPECT_FALSE(RFC1123StrToTime("Sunday, 06-Nov-94 08:49:37 -0100", t));
   EXPECT_FALSE(RFC1123StrToTime("Sunday, 06-Nov-94 08:49:37 -0.1", t));
}
