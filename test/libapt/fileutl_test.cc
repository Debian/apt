#include <config.h>

#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/strutl.h>
#include <apt-pkg/aptconfiguration.h>
#include <apt-pkg/configuration.h>

#include <algorithm>
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
   memset(readback, 'D', sizeof(readback)*sizeof(readback[0])); \
   readback[19] = '\0';
#define EXPECT_N_STR(expect, actual) \
   EXPECT_EQ(0, strncmp(expect, actual, strlen(expect)));
   {
      APT_INIT_READBACK
      char const * const expect = "DDDDDDDDDDDDDDDDDDD";
      EXPECT_STREQ(expect,readback);
      EXPECT_N_STR(expect, readback);
   }
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
   // Non-zero backwards seek
   {
      APT_INIT_READBACK
      char const * const expect = "is";
      EXPECT_EQ(test.size(), f.Tell());
      EXPECT_TRUE(f.Seek(5));
      EXPECT_TRUE(f.Read(readback, strlen(expect)));
      EXPECT_FALSE(f.Failed());
      EXPECT_FALSE(f.Eof());
      EXPECT_N_STR(expect, readback);
      EXPECT_EQ(7, f.Tell());
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
   auto const compressors = APT::Configuration::getCompressors();
   EXPECT_EQ(7, compressors.size());
   bool atLeastOneWasTested = false;
   for (auto const &c: compressors)
   {
      if ((filemode & FileFd::ReadWrite) == FileFd::ReadWrite &&
	    (c.Name.empty() != true && c.Binary.empty() != true))
	 continue;
      atLeastOneWasTested = true;
      TestFileFd(0002, 0664, filemode, c);
      TestFileFd(0022, 0644, filemode, c);
      TestFileFd(0077, 0600, filemode, c);
      TestFileFd(0026, 0640, filemode, c);
   }
   EXPECT_TRUE(atLeastOneWasTested);
}

TEST(FileUtlTest, FileFD)
{
   // testing the (un)compress via pipe, as the 'real' compressors are usually built in via libraries
   _config->Set("APT::Compressor::rev::Name", "rev");
   _config->Set("APT::Compressor::rev::Extension", ".reversed");
   _config->Set("APT::Compressor::rev::Binary", "rev");
   _config->Set("APT::Compressor::rev::Cost", 10);
   auto const compressors = APT::Configuration::getCompressors(false);
   EXPECT_EQ(7, compressors.size());
   EXPECT_TRUE(std::any_of(compressors.begin(), compressors.end(), [](APT::Configuration::Compressor const &c) { return c.Name == "rev"; }));

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
   files = Glob("*MakeLists.txt");
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

   // root can access everything, so /usr will be accepted
   if (geteuid() != 0)
   {
       // here but not accessible for non-roots
       setenv("TMPDIR", "/usr", 1);
       EXPECT_EQ("/tmp", GetTempDir());
   }

   // files are no good for tmpdirs, too
   setenv("TMPDIR", "/dev/null", 1);
   EXPECT_EQ("/tmp", GetTempDir());

   setenv("TMPDIR", "/var/tmp", 1);
   EXPECT_EQ("/var/tmp", GetTempDir());

   unsetenv("TMPDIR");
   if (old_tmpdir.empty() == false)
      setenv("TMPDIR", old_tmpdir.c_str(), 1);
}
TEST(FileUtlTest, Popen)
{
   FileFd Fd;
   pid_t Child;
   char buf[1024];
   std::string s;
   unsigned long long n = 0;
   std::vector<std::string> OpenFds;

   // count Fds to ensure we don't have a resource leak
   if(FileExists("/proc/self/fd"))
      OpenFds = Glob("/proc/self/fd/*");

   // output something
   const char* Args[10] = {"/bin/echo", "meepmeep", NULL};
   EXPECT_TRUE(Popen(Args, Fd, Child, FileFd::ReadOnly));
   EXPECT_TRUE(Fd.Read(buf, sizeof(buf)-1, &n));
   buf[n] = 0;
   EXPECT_NE(n, 0);
   EXPECT_STREQ(buf, "meepmeep\n");

   // wait for the child to exit and cleanup
   EXPECT_TRUE(ExecWait(Child, "PopenRead"));
   EXPECT_TRUE(Fd.Close());

   // ensure that after a close all is good again
   if(FileExists("/proc/self/fd"))
      EXPECT_EQ(Glob("/proc/self/fd/*").size(), OpenFds.size());

   // ReadWrite is not supported
   _error->PushToStack();
   EXPECT_FALSE(Popen(Args, Fd, Child, FileFd::ReadWrite));
   EXPECT_FALSE(Fd.IsOpen());
   EXPECT_FALSE(Fd.Failed());
   EXPECT_TRUE(_error->PendingError());
   _error->RevertToStack();

   // write something
   Args[0] = "/bin/bash";
   Args[1] = "-c";
   Args[2] = "read";
   Args[3] = NULL;
   EXPECT_TRUE(Popen(Args, Fd, Child, FileFd::WriteOnly));
   s = "\n";
   EXPECT_TRUE(Fd.Write(s.c_str(), s.length()));
   EXPECT_TRUE(Fd.Close());
   EXPECT_FALSE(Fd.IsOpen());
   EXPECT_FALSE(Fd.Failed());
   EXPECT_TRUE(ExecWait(Child, "PopenWrite"));
}
TEST(FileUtlTest, flAbsPath)
{
   std::string cwd = SafeGetCWD();
   int res = chdir("/etc/");
   EXPECT_EQ(res, 0);
   std::string p = flAbsPath("passwd");
   EXPECT_EQ(p, "/etc/passwd");

   res = chdir(cwd.c_str());
   EXPECT_EQ(res, 0);
}

static void TestDevNullFileFd(unsigned int const filemode)
{
   SCOPED_TRACE(filemode);
   FileFd f("/dev/null", filemode);
   EXPECT_FALSE(f.Failed());
   EXPECT_TRUE(f.IsOpen());
   EXPECT_TRUE(f.IsOpen());

   std::string test = "This is a test!\n";
   EXPECT_TRUE(f.Write(test.c_str(), test.size()));
   EXPECT_TRUE(f.IsOpen());
   EXPECT_FALSE(f.Failed());

   f.Close();
   EXPECT_FALSE(f.IsOpen());
   EXPECT_FALSE(f.Failed());
}
TEST(FileUtlTest, WorkingWithDevNull)
{
   TestDevNullFileFd(FileFd::WriteOnly | FileFd::Create);
   TestDevNullFileFd(FileFd::WriteOnly | FileFd::Create | FileFd::Empty);
   TestDevNullFileFd(FileFd::WriteOnly | FileFd::Create | FileFd::Exclusive);
   TestDevNullFileFd(FileFd::WriteOnly | FileFd::Atomic);
   TestDevNullFileFd(FileFd::WriteOnly | FileFd::Create | FileFd::Atomic);
   // short-hands for ReadWrite with these modes
   TestDevNullFileFd(FileFd::WriteEmpty);
   TestDevNullFileFd(FileFd::WriteAny);
   TestDevNullFileFd(FileFd::WriteTemp);
   TestDevNullFileFd(FileFd::WriteAtomic);
}
constexpr char const * const TESTSTRING = "This is a test";
static void TestFailingAtomicKeepsFile(char const * const label, std::string const &filename)
{
   SCOPED_TRACE(label);
   EXPECT_TRUE(FileExists(filename));
   FileFd fd;
   EXPECT_TRUE(fd.Open(filename, FileFd::ReadOnly));
   char buffer[50];
   EXPECT_NE(nullptr, fd.ReadLine(buffer, sizeof(buffer)));
   EXPECT_STREQ(TESTSTRING, buffer);
}
TEST(FileUtlTest, FailingAtomic)
{
   FileFd fd;
   std::string filename;
   createTemporaryFile("failingatomic", fd, &filename, TESTSTRING);
   TestFailingAtomicKeepsFile("init", filename);

   FileFd f;
   EXPECT_TRUE(f.Open(filename, FileFd::ReadWrite | FileFd::Atomic));
   f.EraseOnFailure();
   EXPECT_FALSE(f.Failed());
   EXPECT_TRUE(f.IsOpen());
   TestFailingAtomicKeepsFile("before-fail", filename);
   EXPECT_TRUE(f.Write("Bad file write", 10));
   f.OpFail();
   EXPECT_TRUE(f.Failed());
   TestFailingAtomicKeepsFile("after-fail", filename);
   EXPECT_TRUE(f.Close());
   TestFailingAtomicKeepsFile("closed", filename);

   if (filename.empty() == false)
      unlink(filename.c_str());
}
