#include <config.h>

#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>

#include <string>
#include <vector>
#include <stdlib.h>
#include <sys/stat.h>

#include "assert.h"

// regression test for permission bug LP: #1304657
static bool
TestFileFdOpenPermissions(mode_t a_umask, mode_t ExpectedFilePermission)
{
   FileFd f;
   struct stat buf;
   static const char* fname = "test.txt";

   umask(a_umask);
   f.Open(fname, FileFd::ReadWrite|FileFd::Atomic);
   f.Close();
   if (stat(fname, &buf) < 0)
   {
      _error->Errno("stat", "failed to stat");
      _error->DumpErrors();
      return false;
   }
   unlink(fname);
   equals(buf.st_mode & 0777, ExpectedFilePermission);
   return true;
}

int main()
{
   std::vector<std::string> files;

   if (TestFileFdOpenPermissions(0002, 0664) == false ||
       TestFileFdOpenPermissions(0022, 0644) == false ||
       TestFileFdOpenPermissions(0077, 0600) == false ||
       TestFileFdOpenPermissions(0026, 0640) == false)
   {
      return 1;
   }

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
