#include <config.h>

#include <apt-pkg/cmndline.h>
#include <apt-pkg/configuration.h>

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

#define EXPECT_CMD(x, ...) { const char * const argv[] = { __VA_ARGS__ }; EXPECT_EQ(x, CLT::AsString(argv, sizeof(argv)/sizeof(argv[0]))); }

TEST(CommandLineTest,SaveInConfig)
{
   EXPECT_CMD("apt-get install -sf",
	 "apt-get", "install", "-sf");
   EXPECT_CMD("apt-cache -s apt -so Debug::test=Test",
	 "apt-cache", "-s", "apt", "-so", "Debug::test=Test");
   EXPECT_CMD("apt-cache -s apt -so Debug::test=\"Das ist ein Test\"",
	 "apt-cache", "-s", "apt", "-so", "Debug::test=Das ist ein Test");
   EXPECT_CMD("apt-cache -s apt --hallo test=1.0",
	 "apt-cache", "-s", "apt", "--hallo", "test=1.0");
}
TEST(CommandLineTest,Parsing)
{
   CommandLine::Args Args[] = {
      { 't', 0, "Test::Worked", 0 },
      { 'z', "zero", "Test::Zero", 0 },
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
}
