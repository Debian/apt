#include <config.h>

#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>
#include <apt-private/private-cmndline.h>

#include <gtest/gtest.h>

class CLT: public CommandLine {
   public:
      std::string static AsString(const char * const * const argv,
	    unsigned int const argc) {
	 std::string const static conf = "Commandline::AsString";
	 _config->Clear(conf);
	 SaveInConfig(argc, argv);
	 return _config->Find(conf);
      }
};

TEST(CommandLineTest,SaveInConfig)
{
#define APT_EXPECT_CMD(x, ...) { const char * const argv[] = { __VA_ARGS__ }; EXPECT_EQ(x, CLT::AsString(argv, sizeof(argv)/sizeof(argv[0]))); }
   APT_EXPECT_CMD("apt-get install -sf",
	 "apt-get", "install", "-sf");
   APT_EXPECT_CMD("apt-cache -s apt -so Debug::test=Test",
	 "apt-cache", "-s", "apt", "-so", "Debug::test=Test");
   APT_EXPECT_CMD("apt-cache -s apt -so Debug::test='Das ist ein Test'",
	 "apt-cache", "-s", "apt", "-so", "Debug::test=Das ist ein Test");
   APT_EXPECT_CMD("apt-cache -s apt -so Debug::test='Das ist ein Test'",
	 "apt-cache", "-s", "apt", "-so", "Debug::test=\"Das ist ein Test\"");
   APT_EXPECT_CMD("apt-cache -s apt -so Debug::test='Das ist ein Test' foo",
	 "apt-cache", "-s", "apt", "-so", "\"Debug::test=Das ist ein Test\"", "foo");
   APT_EXPECT_CMD("apt-cache -s apt -so Debug::test='Das ist ein Test' foo",
	 "apt-cache", "-s", "apt", "-so", "\'Debug::test=Das ist ein Test\'", "foo");
   APT_EXPECT_CMD("apt-cache -s apt -so Debug::test='That 	 is crazy!' foo",
	 "apt-cache", "-s", "apt", "-so", "\'Debug::test=That 	 \ris\n crazy!\'", "foo");
   APT_EXPECT_CMD("apt-cache -s apt --hallo test=1.0",
	 "apt-cache", "-s", "apt", "--hallo", "test=1.0");
#undef APT_EXPECT_CMD
}
TEST(CommandLineTest,Parsing)
{
   CommandLine::Args Args[] = {
      { 't', 0, "Test::Worked", 0 },
      { 'T', "testing", "Test::Worked", CommandLine::HasArg },
      { 'z', "zero", "Test::Zero", 0 },
      { 'o', "option", 0, CommandLine::ArbItem },
      {0,0,0,0}
   };
   ::Configuration c;
   CommandLine CmdL(Args, &c);

   char const * argv[] = { "test", "--zero", "-t" };
   CmdL.Parse(3 , argv);
   EXPECT_TRUE(c.FindB("Test::Worked", false));
   EXPECT_TRUE(c.FindB("Test::Zero", false));

   c.Clear("Test");
   EXPECT_FALSE(c.FindB("Test::Worked", false));
   EXPECT_FALSE(c.FindB("Test::Zero", false));

   c.Set("Test::Zero", true);
   EXPECT_TRUE(c.FindB("Test::Zero", false));

   char const * argv2[] = { "test", "--no-zero", "-t" };
   CmdL.Parse(3 , argv2);
   EXPECT_TRUE(c.FindB("Test::Worked", false));
   EXPECT_FALSE(c.FindB("Test::Zero", false));

   c.Clear("Test");
   {
   char const * argv[] = { "test", "-T", "yes" };
   CmdL.Parse(3 , argv);
   EXPECT_TRUE(c.FindB("Test::Worked", false));
   EXPECT_EQ("yes", c.Find("Test::Worked", "no"));
   EXPECT_EQ(0u, CmdL.FileSize());
   }
   c.Clear("Test");
   {
   char const * argv[] = { "test", "-T=yes" };
   CmdL.Parse(2 , argv);
   EXPECT_TRUE(c.Exists("Test::Worked"));
   EXPECT_EQ("yes", c.Find("Test::Worked", "no"));
   EXPECT_EQ(0u, CmdL.FileSize());
   }
   c.Clear("Test");
   {
   char const * argv[] = { "test", "-T=", "yes" };
   CmdL.Parse(3 , argv);
   EXPECT_TRUE(c.Exists("Test::Worked"));
   EXPECT_EQ("no", c.Find("Test::Worked", "no"));
   EXPECT_EQ(1u, CmdL.FileSize());
   }

   c.Clear("Test");
   {
   char const * argv[] = { "test", "--testing", "yes" };
   CmdL.Parse(3 , argv);
   EXPECT_TRUE(c.FindB("Test::Worked", false));
   EXPECT_EQ("yes", c.Find("Test::Worked", "no"));
   EXPECT_EQ(0u, CmdL.FileSize());
   }
   c.Clear("Test");
   {
   char const * argv[] = { "test", "--testing=yes" };
   CmdL.Parse(2 , argv);
   EXPECT_TRUE(c.Exists("Test::Worked"));
   EXPECT_EQ("yes", c.Find("Test::Worked", "no"));
   EXPECT_EQ(0u, CmdL.FileSize());
   }
   c.Clear("Test");
   {
   char const * argv[] = { "test", "--testing=", "yes" };
   CmdL.Parse(3 , argv);
   EXPECT_TRUE(c.Exists("Test::Worked"));
   EXPECT_EQ("no", c.Find("Test::Worked", "no"));
   EXPECT_EQ(1u, CmdL.FileSize());
   }

   c.Clear("Test");
   {
   char const * argv[] = { "test", "-o", "test::worked=yes" };
   CmdL.Parse(3 , argv);
   EXPECT_TRUE(c.FindB("Test::Worked", false));
   EXPECT_EQ("yes", c.Find("Test::Worked", "no"));
   }
   c.Clear("Test");
   {
   char const * argv[] = { "test", "-o", "test::worked=" };
   CmdL.Parse(3 , argv);
   EXPECT_TRUE(c.Exists("Test::Worked"));
   EXPECT_EQ("no", c.Find("Test::Worked", "no"));
   }
   c.Clear("Test");
   {
   char const * argv[] = { "test", "-o", "test::worked=", "yes" };
   CmdL.Parse(4 , argv);
   EXPECT_TRUE(c.Exists("Test::Worked"));
   EXPECT_EQ("no", c.Find("Test::Worked", "no"));
   }
   c.Clear("Test");
}

TEST(CommandLineTest, BoolParsing)
{
   CommandLine::Args Args[] = {
      { 't', 0, "Test::Worked", 0 },
      {0,0,0,0}
   };
   ::Configuration c;
   CommandLine CmdL(Args, &c);

   // the commandline parser used to use strtol() on the argument
   // to check if the argument is a boolean expression - that
   // stopped after the "0". this test ensures that we always check 
   // that the entire string was consumed by strtol
   {
   char const * argv[] = { "show", "-t", "0ad" };
   bool res = CmdL.Parse(sizeof(argv)/sizeof(char*), argv);
   EXPECT_TRUE(res);
   ASSERT_EQ(std::string(CmdL.FileList[0]), "0ad");
   }

   {
   char const * argv[] = { "show", "-t", "0", "ad" };
   bool res = CmdL.Parse(sizeof(argv)/sizeof(char*), argv);
   EXPECT_TRUE(res);
   ASSERT_EQ(std::string(CmdL.FileList[0]), "ad");
   }

}

static bool DoVoid(CommandLine &) { return false; }

TEST(CommandLineTest,GetCommand)
{
   CommandLine::Dispatch Cmds[] = { {"install",&DoVoid}, {"remove", &DoVoid}, {0,0} };
   {
   char const * argv[] = { "apt-get", "-t", "unstable", "remove", "-d", "foo" };
   char const * com = CommandLine::GetCommand(Cmds, sizeof(argv)/sizeof(argv[0]), argv);
   EXPECT_STREQ("remove", com);
   std::vector<CommandLine::Args> Args = getCommandArgs(APT_CMD::APT_GET, com);
   ::Configuration c;
   CommandLine CmdL(Args.data(), &c);
   ASSERT_TRUE(CmdL.Parse(sizeof(argv)/sizeof(argv[0]), argv));
   EXPECT_EQ(c.Find("APT::Default-Release"), "unstable");
   EXPECT_TRUE(c.FindB("APT::Get::Download-Only"));
   ASSERT_EQ(2u, CmdL.FileSize());
   EXPECT_EQ(std::string(CmdL.FileList[0]), "remove");
   EXPECT_EQ(std::string(CmdL.FileList[1]), "foo");
   }
   {
   char const * argv[] = {"apt-get", "-t", "unstable", "remove", "--", "-d", "foo" };
   char const * com = CommandLine::GetCommand(Cmds, sizeof(argv)/sizeof(argv[0]), argv);
   EXPECT_STREQ("remove", com);
   std::vector<CommandLine::Args> Args = getCommandArgs(APT_CMD::APT_GET, com);
   ::Configuration c;
   CommandLine CmdL(Args.data(), &c);
   ASSERT_TRUE(CmdL.Parse(sizeof(argv)/sizeof(argv[0]), argv));
   EXPECT_EQ(c.Find("APT::Default-Release"), "unstable");
   EXPECT_FALSE(c.FindB("APT::Get::Download-Only"));
   ASSERT_EQ(3u, CmdL.FileSize());
   EXPECT_EQ(std::string(CmdL.FileList[0]), "remove");
   EXPECT_EQ(std::string(CmdL.FileList[1]), "-d");
   EXPECT_EQ(std::string(CmdL.FileList[2]), "foo");
   }
   {
   char const * argv[] = {"apt-get", "-t", "unstable", "--", "remove", "-d", "foo" };
   char const * com = CommandLine::GetCommand(Cmds, sizeof(argv)/sizeof(argv[0]), argv);
   EXPECT_STREQ("remove", com);
   std::vector<CommandLine::Args> Args = getCommandArgs(APT_CMD::APT_GET, com);
   ::Configuration c;
   CommandLine CmdL(Args.data(), &c);
   ASSERT_TRUE(CmdL.Parse(sizeof(argv)/sizeof(argv[0]), argv));
   EXPECT_EQ(c.Find("APT::Default-Release"), "unstable");
   EXPECT_FALSE(c.FindB("APT::Get::Download-Only"));
   ASSERT_EQ(3u, CmdL.FileSize());
   EXPECT_EQ(std::string(CmdL.FileList[0]), "remove");
   EXPECT_EQ(std::string(CmdL.FileList[1]), "-d");
   EXPECT_EQ(std::string(CmdL.FileList[2]), "foo");
   }
   {
   char const * argv[] = {"apt-get", "install", "-t", "unstable", "--", "remove", "-d", "foo" };
   char const * com = CommandLine::GetCommand(Cmds, sizeof(argv)/sizeof(argv[0]), argv);
   EXPECT_STREQ("install", com);
   std::vector<CommandLine::Args> Args = getCommandArgs(APT_CMD::APT_GET, com);
   ::Configuration c;
   CommandLine CmdL(Args.data(), &c);
   ASSERT_TRUE(CmdL.Parse(sizeof(argv)/sizeof(argv[0]), argv));
   EXPECT_EQ(c.Find("APT::Default-Release"), "unstable");
   EXPECT_FALSE(c.FindB("APT::Get::Download-Only"));
   ASSERT_EQ(4u, CmdL.FileSize());
   EXPECT_EQ(std::string(CmdL.FileList[0]), "install");
   EXPECT_EQ(std::string(CmdL.FileList[1]), "remove");
   EXPECT_EQ(std::string(CmdL.FileList[2]), "-d");
   EXPECT_EQ(std::string(CmdL.FileList[3]), "foo");
   }
}
