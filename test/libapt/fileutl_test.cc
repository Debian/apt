#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>

#include "assert.h"
#include <string>
#include <vector>

#include <stdio.h>
#include <iostream>
#include <stdlib.h>


int main()
{
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
