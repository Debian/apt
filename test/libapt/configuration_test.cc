#include <config.h>

#include <apt-pkg/configuration.h>
#include <apt-pkg/fileutl.h>

#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "file-helpers.h"

TEST(ConfigurationTest,Lists)
{
	Configuration Cnf;

	Cnf.Set("APT::Keep-Fds::",28);
	Cnf.Set("APT::Keep-Fds::",17);
	Cnf.Set("APT::Keep-Fds::2",47);
	Cnf.Set("APT::Keep-Fds::","broken");
	std::vector<std::string> fds = Cnf.FindVector("APT::Keep-Fds");
	ASSERT_EQ(4u, fds.size());
	EXPECT_EQ("28", fds[0]);
	EXPECT_EQ("17", fds[1]);
	EXPECT_EQ("47", fds[2]);
	EXPECT_EQ("broken", fds[3]);

	EXPECT_TRUE(Cnf.Exists("APT::Keep-Fds::2"));
	EXPECT_EQ("47", Cnf.Find("APT::Keep-Fds::2"));
	EXPECT_EQ(47, Cnf.FindI("APT::Keep-Fds::2"));
	EXPECT_FALSE(Cnf.Exists("APT::Keep-Fds::3"));
	EXPECT_EQ("", Cnf.Find("APT::Keep-Fds::3"));
	EXPECT_EQ(56, Cnf.FindI("APT::Keep-Fds::3", 56));
	EXPECT_EQ("not-set", Cnf.Find("APT::Keep-Fds::3", "not-set"));

	Cnf.Clear("APT::Keep-Fds::2");
	EXPECT_TRUE(Cnf.Exists("APT::Keep-Fds::2"));
	fds = Cnf.FindVector("APT::Keep-Fds");
	ASSERT_EQ(4u, fds.size());
	EXPECT_EQ("28", fds[0]);
	EXPECT_EQ("17", fds[1]);
	EXPECT_EQ("", fds[2]);
	EXPECT_EQ("broken", fds[3]);

	Cnf.Clear("APT::Keep-Fds",28);
	fds = Cnf.FindVector("APT::Keep-Fds");
	ASSERT_EQ(3u, fds.size());
	EXPECT_EQ("17", fds[0]);
	EXPECT_EQ("", fds[1]);
	EXPECT_EQ("broken", fds[2]);

	Cnf.Clear("APT::Keep-Fds","");
	EXPECT_FALSE(Cnf.Exists("APT::Keep-Fds::2"));

	Cnf.Clear("APT::Keep-Fds",17);
	Cnf.Clear("APT::Keep-Fds","broken");
	fds = Cnf.FindVector("APT::Keep-Fds");
	EXPECT_TRUE(fds.empty());

	Cnf.Set("APT::Keep-Fds::",21);
	Cnf.Set("APT::Keep-Fds::",42);
	fds = Cnf.FindVector("APT::Keep-Fds");
	ASSERT_EQ(2u, fds.size());
	EXPECT_EQ("21", fds[0]);
	EXPECT_EQ("42", fds[1]);

	Cnf.Clear("APT::Keep-Fds");
	fds = Cnf.FindVector("APT::Keep-Fds");
	EXPECT_TRUE(fds.empty());
}
TEST(ConfigurationTest,Integers)
{
	Configuration Cnf;

	Cnf.CndSet("APT::Version", 42);
	Cnf.CndSet("APT::Version", "66");
	EXPECT_EQ("42", Cnf.Find("APT::Version"));
	EXPECT_EQ(42, Cnf.FindI("APT::Version"));
	EXPECT_EQ("42", Cnf.Find("APT::Version", "33"));
	EXPECT_EQ(42, Cnf.FindI("APT::Version", 33));
	EXPECT_EQ("33", Cnf.Find("APT2::Version", "33"));
	EXPECT_EQ(33, Cnf.FindI("APT2::Version", 33));
}
TEST(ConfigurationTest,DirsAndFiles)
{
	Configuration Cnf;

	EXPECT_EQ("", Cnf.FindFile("Dir::State"));
	EXPECT_EQ("", Cnf.FindFile("Dir::Aptitude::State"));
	Cnf.Set("Dir", "/srv/sid");
	EXPECT_EQ("", Cnf.FindFile("Dir::State"));
	Cnf.Set("Dir::State", "var/lib/apt");
	Cnf.Set("Dir::Aptitude::State", "var/lib/aptitude");
	EXPECT_EQ("/srv/sid/var/lib/apt", Cnf.FindFile("Dir::State"));
	EXPECT_EQ("/srv/sid/var/lib/aptitude", Cnf.FindFile("Dir::Aptitude::State"));

	Cnf.Set("RootDir", "/");
	EXPECT_EQ("/srv/sid/var/lib/apt", Cnf.FindFile("Dir::State"));
	EXPECT_EQ("/srv/sid/var/lib/aptitude", Cnf.FindFile("Dir::Aptitude::State"));
	Cnf.Set("RootDir", "//./////.////");
	EXPECT_EQ("/srv/sid/var/lib/apt", Cnf.FindFile("Dir::State"));
	EXPECT_EQ("/srv/sid/var/lib/aptitude", Cnf.FindFile("Dir::Aptitude::State"));
	Cnf.Set("RootDir", "/rootdir");
	EXPECT_EQ("/rootdir/srv/sid/var/lib/apt", Cnf.FindFile("Dir::State"));
	EXPECT_EQ("/rootdir/srv/sid/var/lib/aptitude", Cnf.FindFile("Dir::Aptitude::State"));
	Cnf.Set("RootDir", "/rootdir/");
	EXPECT_EQ("/rootdir/srv/sid/var/lib/apt", Cnf.FindFile("Dir::State"));
	EXPECT_EQ("/rootdir/srv/sid/var/lib/aptitude", Cnf.FindFile("Dir::Aptitude::State"));

	Cnf.Set("Dir::State", "/dev/null");
	Cnf.Set("Dir::State::lists", "lists/");
	EXPECT_EQ("/rootdir/dev/null", Cnf.FindDir("Dir::State"));
	EXPECT_EQ("/rootdir/dev/null", Cnf.FindDir("Dir::State::lists"));
}
TEST(ConfigurationTest,DevNullInPaths)
{
	Configuration Cnf;
	EXPECT_EQ("", Cnf.FindFile("Dir"));
	EXPECT_EQ("", Cnf.FindFile("Dir::State"));
	EXPECT_EQ("", Cnf.FindFile("Dir::State::status"));
	Cnf.Set("Dir::State", "/dev/null");
	EXPECT_EQ("/dev/null", Cnf.FindFile("Dir::State"));
	Cnf.Set("Dir", "/");
	Cnf.Set("Dir::State::status", "status");
	EXPECT_EQ("/dev/null", Cnf.FindFile("Dir::State"));
	EXPECT_EQ("/dev/null", Cnf.FindFile("Dir::State::status"));
	Cnf.Set("Dir::State", "");
	EXPECT_EQ("", Cnf.FindFile("Dir::State"));
	EXPECT_EQ("/status", Cnf.FindFile("Dir::State::status"));
	Cnf.Set("Dir", "/dev/null");
	EXPECT_EQ("/dev/null", Cnf.FindFile("Dir"));
	EXPECT_EQ("", Cnf.FindFile("Dir::State"));
	EXPECT_EQ("/dev/null", Cnf.FindFile("Dir::State::status"));
	Cnf.Set("Dir", "/rootdir");
	EXPECT_EQ("/rootdir", Cnf.FindFile("Dir"));
	EXPECT_EQ("", Cnf.FindFile("Dir::State"));
	EXPECT_EQ("/rootdir/status", Cnf.FindFile("Dir::State::status"));
	Cnf.Set("Dir::State::status", "/foo/status");
	EXPECT_EQ("/rootdir", Cnf.FindFile("Dir"));
	EXPECT_EQ("", Cnf.FindFile("Dir::State"));
	EXPECT_EQ("/foo/status", Cnf.FindFile("Dir::State::status"));
}
TEST(ConfigurationTest,Vector)
{
	Configuration Cnf;

	std::vector<std::string> vec = Cnf.FindVector("Test::Vector", "");
	EXPECT_EQ(0u, vec.size());
	vec = Cnf.FindVector("Test::Vector", "foo");
	ASSERT_EQ(1u, vec.size());
	EXPECT_EQ("foo", vec[0]);
	vec = Cnf.FindVector("Test::Vector", "foo,bar");
	EXPECT_EQ(2u, vec.size());
	EXPECT_EQ("foo", vec[0]);
	EXPECT_EQ("bar", vec[1]);
	Cnf.Set("Test::Vector::", "baz");
	Cnf.Set("Test::Vector::", "bob");
	Cnf.Set("Test::Vector::", "dob");
	vec = Cnf.FindVector("Test::Vector");
	ASSERT_EQ(3u, vec.size());
	EXPECT_EQ("baz", vec[0]);
	EXPECT_EQ("bob", vec[1]);
	EXPECT_EQ("dob", vec[2]);
	vec = Cnf.FindVector("Test::Vector", "foo,bar");
	ASSERT_EQ(3u, vec.size());
	EXPECT_EQ("baz", vec[0]);
	EXPECT_EQ("bob", vec[1]);
	EXPECT_EQ("dob", vec[2]);
	Cnf.Set("Test::Vector", "abel,bravo");
	vec = Cnf.FindVector("Test::Vector", "foo,bar");
	ASSERT_EQ(2u, vec.size());
	EXPECT_EQ("abel", vec[0]);
	EXPECT_EQ("bravo", vec[1]);
}
TEST(ConfigurationTest,Merge)
{
	Configuration Cnf;
	Cnf.Set("Binary::apt::option::foo", "bar");
	Cnf.Set("Binary::apt::option::empty", "");
	Cnf.Set("option::foo", "foo");

	Cnf.MoveSubTree("Binary::apt", "Binary::apt2");
	EXPECT_FALSE(Cnf.Exists("Binary::apt::option"));
	EXPECT_TRUE(Cnf.Exists("option"));
	EXPECT_EQ("foo", Cnf.Find("option::foo"));
	EXPECT_EQ("bar", Cnf.Find("Binary::apt2::option::foo"));

	EXPECT_FALSE(Cnf.Exists("option::empty"));
	EXPECT_TRUE(Cnf.Exists("Binary::apt2::option::empty"));
	Cnf.Set("option::empty", "not");

	Cnf.MoveSubTree("Binary::apt2", NULL);
	EXPECT_FALSE(Cnf.Exists("Binary::apt2::option"));
	EXPECT_TRUE(Cnf.Exists("option"));
	EXPECT_EQ("bar", Cnf.Find("option::foo"));
	EXPECT_EQ("", Cnf.Find("option::empty"));
}
TEST(ConfigurationTest, Parsing)
{
   Configuration Cnf;
   {
      auto const file = createTemporaryFile("doublesignedfile", R"apt(
SimpleOption "true";
/* SimpleOption "false"; */
Answer::Simple "42";
# This is a comment
List::Option { "In"; "One"; "Line"; };
// this a comment as well
List::Option2 { "Multi";
"Line"; # inline comment
	 "Options";
}; Trailing "true";
/* Commented::Out "true"; */
)apt");
      EXPECT_TRUE(ReadConfigFile(Cnf, file.Name()));
   }
   EXPECT_TRUE(Cnf.FindB("SimpleOption"));
   EXPECT_EQ(42, Cnf.FindI("Answer::Simple"));
   EXPECT_TRUE(Cnf.Exists("List::Option"));
   auto const singleline = Cnf.FindVector("List::Option");
   EXPECT_EQ(3u, singleline.size());
   EXPECT_EQ("In", singleline[0]);
   EXPECT_EQ("One", singleline[1]);
   EXPECT_EQ("Line", singleline[2]);
   auto const multiline = Cnf.FindVector("List::Option2");
   EXPECT_EQ(3u, multiline.size());
   EXPECT_EQ("Multi", multiline[0]);
   EXPECT_EQ("Line", multiline[1]);
   EXPECT_EQ("Options", multiline[2]);
   EXPECT_TRUE(Cnf.FindB("Trailing"));
   EXPECT_FALSE(Cnf.Exists("Commented::Out"));
}
