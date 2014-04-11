#include <config.h>

#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>
#include <apt-pkg/aptconfiguration.h>

#include <string>
#include <vector>
#include <stdlib.h>
#include <sys/stat.h>
#include <string.h>

#include "assert.h"

static void assertStringEquals(char const * const expect, char const * const got, unsigned long const line) {
	if (strncmp(expect, got, strlen(expect)) == 0)
		return;
	OutputAssertEqual(expect, "==", got, line);
}
#define strequals(x,y) assertStringEquals(x, y, __LINE__)

static bool
TestFileFd(mode_t const a_umask, mode_t const ExpectedFilePermission, unsigned int const filemode, APT::Configuration::Compressor const &compressor)
{
   FileFd f;
   struct stat buf;
   static const char* fname = "test.txt";

   umask(a_umask);
   equals(f.Open(fname, filemode, compressor), true);
   equals(f.IsOpen(), true);
   equals(f.Failed(), false);
   equals(umask(a_umask), a_umask);

   std::string test = "This is a test!";
   equals(f.Write(test.c_str(), test.size()), true);
   equals(f.IsOpen(), true);
   equals(f.Failed(), false);

   f.Close();
   equals(f.IsOpen(), false);
   equals(f.Failed(), false);

   equals(f.Open(fname, FileFd::ReadOnly, compressor), true);
   equalsNot(f.FileSize(), 0);
   equals(f.IsOpen(), true);
   equals(f.Failed(), false);

   char readback[20];
   {
      char const * const expect = "This";
      equals(f.Read(readback, strlen(expect)), true);
      equals(f.Failed(), false);
      equals(f.Eof(), false);
      strequals(expect, readback);
      equals(strlen(expect), f.Tell());
   }
   {
      char const * const expect = "test!";
      equals(f.Skip((test.size() - f.Tell()) - strlen(expect)), true);
      equals(f.Read(readback, strlen(expect)), true);
      equals(f.Failed(), false);
      equals(f.Eof(), false);
      strequals(expect, readback);
      equals(test.size(), f.Tell());
   }

   equals(f.Seek(0), true);
   equals(f.Read(readback, 20, true), true);
   equals(f.Failed(), false);
   equals(f.Eof(), true);
   equals(test, readback);
   equals(test.size(), strlen(readback));
   equals(f.Size(), f.Tell());

   equals(f.Seek(0), true);
   f.ReadLine(readback, 20);
   equals(f.Failed(), false);
   equals(f.Eof(), true);
   equals(test, readback);
   equals(test.size(), strlen(readback));
   equals(f.Size(), f.Tell());

   f.Close();
   equals(f.IsOpen(), false);
   equals(f.Failed(), false);

   // regression test for permission bug LP: #1304657
   if (stat(fname, &buf) < 0)
   {
      _error->Errno("stat", "failed to stat");
      return false;
   }
   unlink(fname);
   equals(buf.st_mode & 0777, ExpectedFilePermission);
   return true;
}

static bool TestFileFd(unsigned int const filemode)
{
   std::vector<APT::Configuration::Compressor> const compressors = APT::Configuration::getCompressors();
   for (std::vector<APT::Configuration::Compressor>::const_iterator c = compressors.begin(); c != compressors.end(); ++c)
   {
      if ((filemode & FileFd::ReadWrite) == FileFd::ReadWrite &&
	    (c->Name.empty() != true && c->Binary.empty() != true))
	 continue;
      if (TestFileFd(0002, 0664, filemode, *c) == false ||
	    TestFileFd(0022, 0644, filemode, *c) == false ||
	    TestFileFd(0077, 0600, filemode, *c) == false ||
	    TestFileFd(0026, 0640, filemode, *c) == false)
      {
	 _error->DumpErrors();
	 return false;
      }
   }
   return true;
}

int main()
{
   if (TestFileFd(FileFd::WriteOnly | FileFd::Create) == false ||
	 TestFileFd(FileFd::WriteOnly | FileFd::Create | FileFd::Empty) == false ||
	 TestFileFd(FileFd::WriteOnly | FileFd::Create | FileFd::Exclusive) == false ||
	 TestFileFd(FileFd::WriteOnly | FileFd::Atomic) == false ||
	 TestFileFd(FileFd::WriteOnly | FileFd::Create | FileFd::Atomic) == false ||
	 // short-hands for ReadWrite with these modes
	 TestFileFd(FileFd::WriteEmpty) == false ||
	 TestFileFd(FileFd::WriteAny) == false ||
	 TestFileFd(FileFd::WriteTemp) == false ||
	 TestFileFd(FileFd::WriteAtomic) == false)
   {
      return 1;
   }

   std::vector<std::string> files;
   // normal match
   files = Glob("*.lst");
   if (files.size() != 1)
   {
      _error->DumpErrors();
      return 1;
   }

   // not there
   files = Glob("xxxyyyzzz");
   if (files.size() != 0 || _error->PendingError())
   {
      _error->DumpErrors();
      return 1;
   }

   // many matches (number is a bit random)
   files = Glob("*.cc");
   if (files.size() < 10)
   {
      _error->DumpErrors();
      return 1;
   }

   // GetTempDir()
   unsetenv("TMPDIR");
   equals(GetTempDir(), "/tmp");

   setenv("TMPDIR", "", 1);
   equals(GetTempDir(), "/tmp");

   setenv("TMPDIR", "/not-there-no-really-not", 1);
   equals(GetTempDir(), "/tmp");

   setenv("TMPDIR", "/usr", 1);
   equals(GetTempDir(), "/usr");

   return 0;
}
