#include <config.h>

#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/aptconfiguration.h>

#include <string>
#include <vector>
#include <stdlib.h>
#include <string.h>

#include <gtest/gtest.h>

#include "file-helpers.h"

static void TestFileFd(mode_t const a_umask, mode_t const ExpectedFilePermission,
      unsigned int const filemode, APT::Configuration::Compressor const &compressor)
{
   std::string trace;
   strprintf(trace, "TestFileFd: Compressor: %s umask: %#o permission: %#o mode: %d", compressor.Name.c_str(), a_umask, ExpectedFilePermission, filemode);
   SCOPED_TRACE(trace);

   static const char* fname = "apt-filefd-test.txt";
   if (FileExists(fname) == true)
      EXPECT_EQ(0, unlink(fname));

   FileFd f;
   umask(a_umask);
   EXPECT_TRUE(f.Open(fname, filemode, compressor));
   EXPECT_TRUE(f.IsOpen());
   EXPECT_FALSE(f.Failed());
   EXPECT_EQ(umask(a_umask), a_umask);

   std::string test = "This is a test!\n";
   EXPECT_TRUE(f.Write(test.c_str(), test.size()));
   EXPECT_TRUE(f.IsOpen());
   EXPECT_FALSE(f.Failed());

   f.Close();
   EXPECT_FALSE(f.IsOpen());
   EXPECT_FALSE(f.Failed());

   EXPECT_TRUE(f.Open(fname, FileFd::ReadOnly, compressor));
   EXPECT_TRUE(f.IsOpen());
   EXPECT_FALSE(f.Failed());
   EXPECT_FALSE(f.Eof());
   EXPECT_NE(0, f.FileSize());
   EXPECT_FALSE(f.Failed());
   EXPECT_NE(0, f.ModificationTime());
   EXPECT_FALSE(f.Failed());

   // ensure the memory is as predictably messed up
#define APT_INIT_READBACK \
   char readback[20]; \
   memset(readback, 'D', sizeof(readback)/sizeof(readback[0])); \
   readback[19] = '\0';
#define EXPECT_N_STR(expect, actual) \
   EXPECT_EQ(0, strncmp(expect, actual, strlen(expect)));

   {
      APT_INIT_READBACK
      char const * const expect = "This";
      EXPECT_TRUE(f.Read(readback, strlen(expect)));
      EXPECT_FALSE(f.Failed());
      EXPECT_FALSE(f.Eof());
      EXPECT_N_STR(expect, readback);
      EXPECT_EQ(strlen(expect), f.Tell());
   }
   {
      APT_INIT_READBACK
      char const * const expect = "test!\n";
      EXPECT_TRUE(f.Skip((test.size() - f.Tell()) - strlen(expect)));
      EXPECT_TRUE(f.Read(readback, strlen(expect)));
      EXPECT_FALSE(f.Failed());
      EXPECT_FALSE(f.Eof());
      EXPECT_N_STR(expect, readback);
      EXPECT_EQ(test.size(), f.Tell());
   }
   {
      APT_INIT_READBACK
      EXPECT_TRUE(f.Seek(0));
      EXPECT_FALSE(f.Eof());
      EXPECT_TRUE(f.Read(readback, 20, true));
      EXPECT_FALSE(f.Failed());
      EXPECT_TRUE(f.Eof());
      EXPECT_N_STR(test.c_str(), readback);
      EXPECT_EQ(f.Size(), f.Tell());
   }
   {
      APT_INIT_READBACK
      EXPECT_TRUE(f.Seek(0));
      EXPECT_FALSE(f.Eof());
      EXPECT_TRUE(f.Read(readback, test.size(), true));
      EXPECT_FALSE(f.Failed());
      EXPECT_FALSE(f.Eof());
      EXPECT_N_STR(test.c_str(), readback);
      EXPECT_EQ(f.Size(), f.Tell());
   }
   {
      APT_INIT_READBACK
      EXPECT_TRUE(f.Seek(0));
      EXPECT_FALSE(f.Eof());
      unsigned long long actual;
      EXPECT_TRUE(f.Read(readback, 20, &actual));
      EXPECT_FALSE(f.Failed());
      EXPECT_TRUE(f.Eof());
      EXPECT_EQ(test.size(), actual);
      EXPECT_N_STR(test.c_str(), readback);
      EXPECT_EQ(f.Size(), f.Tell());
   }
   {
      APT_INIT_READBACK
      EXPECT_TRUE(f.Seek(0));
      EXPECT_FALSE(f.Eof());
      f.ReadLine(readback, 20);
      EXPECT_FALSE(f.Failed());
      EXPECT_FALSE(f.Eof());
      EXPECT_EQ(test, readback);
      EXPECT_EQ(f.Size(), f.Tell());
   }
   {
      APT_INIT_READBACK
      EXPECT_TRUE(f.Seek(0));
      EXPECT_FALSE(f.Eof());
      char const * const expect = "This";
      f.ReadLine(readback, strlen(expect) + 1);
      EXPECT_FALSE(f.Failed());
      EXPECT_FALSE(f.Eof());
      EXPECT_N_STR(expect, readback);
      EXPECT_EQ(strlen(expect), f.Tell());
   }
#undef APT_INIT_READBACK

   f.Close();
   EXPECT_FALSE(f.IsOpen());
   EXPECT_FALSE(f.Failed());

   // regression test for permission bug LP: #1304657
   struct stat buf;
   EXPECT_EQ(0, stat(fname, &buf));
   EXPECT_EQ(0, unlink(fname));
   EXPECT_EQ(ExpectedFilePermission, buf.st_mode & 0777);
}

static void TestFileFd(unsigned int const filemode)
{
   std::vector<APT::Configuration::Compressor> compressors = APT::Configuration::getCompressors();

   // testing the (un)compress via pipe, as the 'real' compressors are usually built in via libraries
   compressors.push_back(APT::Configuration::Compressor("rev", ".reversed", "rev", NULL, NULL, 42));
   //compressors.push_back(APT::Configuration::Compressor("cat", ".ident", "cat", NULL, NULL, 42));

   for (std::vector<APT::Configuration::Compressor>::const_iterator c = compressors.begin(); c != compressors.end(); ++c)
   {
      if ((filemode & FileFd::ReadWrite) == FileFd::ReadWrite &&
	    (c->Name.empty() != true && c->Binary.empty() != true))
	 continue;
      TestFileFd(0002, 0664, filemode, *c);
      TestFileFd(0022, 0644, filemode, *c);
      TestFileFd(0077, 0600, filemode, *c);
      TestFileFd(0026, 0640, filemode, *c);
   }
}

TEST(FileUtlTest, FileFD)
{
   std::string const startdir = SafeGetCWD();
   EXPECT_FALSE(startdir.empty());
   std::string tempdir;
   createTemporaryDirectory("filefd", tempdir);
   EXPECT_EQ(0, chdir(tempdir.c_str()));

   TestFileFd(FileFd::WriteOnly | FileFd::Create);
   TestFileFd(FileFd::WriteOnly | FileFd::Create | FileFd::Empty);
   TestFileFd(FileFd::WriteOnly | FileFd::Create | FileFd::Exclusive);
   TestFileFd(FileFd::WriteOnly | FileFd::Atomic);
   TestFileFd(FileFd::WriteOnly | FileFd::Create | FileFd::Atomic);
   // short-hands for ReadWrite with these modes
   TestFileFd(FileFd::WriteEmpty);
   TestFileFd(FileFd::WriteAny);
   TestFileFd(FileFd::WriteTemp);
   TestFileFd(FileFd::WriteAtomic);

   EXPECT_EQ(0, chdir(startdir.c_str()));
   removeDirectory(tempdir);
}
TEST(FileUtlTest, Glob)
{
   std::vector<std::string> files;
   // normal match
   files = Glob("*akefile");
   EXPECT_EQ(1, files.size());

   // not there
   files = Glob("xxxyyyzzz");
   EXPECT_TRUE(files.empty());
   EXPECT_FALSE(_error->PendingError());

   // many matches (number is a bit random)
   files = Glob("*.cc");
   EXPECT_LT(10, files.size());
}
TEST(FileUtlTest, GetTempDir)
{
   char const * const envtmp = getenv("TMPDIR");
   std::string old_tmpdir;
   if (envtmp != NULL)
      old_tmpdir = envtmp;

   unsetenv("TMPDIR");
   EXPECT_EQ("/tmp", GetTempDir());

   setenv("TMPDIR", "", 1);
   EXPECT_EQ("/tmp", GetTempDir());

   setenv("TMPDIR", "/not-there-no-really-not", 1);
   EXPECT_EQ("/tmp", GetTempDir());

   setenv("TMPDIR", "/usr", 1);
   EXPECT_EQ("/usr", GetTempDir());

   unsetenv("TMPDIR");
   if (old_tmpdir.empty() == false)
      setenv("TMPDIR", old_tmpdir.c_str(), 1);
}
