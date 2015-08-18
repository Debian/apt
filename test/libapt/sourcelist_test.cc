#include <config.h>

#include <apt-pkg/sourcelist.h>
#include <apt-pkg/fileutl.h>

#include <string>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include "file-helpers.h"

TEST(SourceListTest,ParseFileDeb822)
{
   FileFd fd;
   std::string tempfile;
   createTemporaryFile("parsefiledeb822.XXXXXX.sources", fd, &tempfile,
      "Types: deb\n"
      "URIs: http://ftp.debian.org/debian\n"
      "Suites: stable\n"
      "Components: main\n"
      "Description: short\n"
      " long description that can be very long\n"
      "\n"
      "Types: deb\n"
      "URIs: http://ftp.debian.org/debian\n"
      "Suites: unstable\n"
      "Components: main non-free\n");
   fd.Close();

   pkgSourceList sources;
   EXPECT_EQ(true, sources.Read(tempfile));
   EXPECT_EQ(2, sources.size());

   if (tempfile.empty() == false)
      unlink(tempfile.c_str());
}
