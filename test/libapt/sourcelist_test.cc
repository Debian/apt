#include <config.h>

#include <apt-pkg/fileutl.h>
#include <apt-pkg/sourcelist.h>

#include <string>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include "file-helpers.h"

TEST(SourceListTest,ParseFileDeb822)
{
   auto const file = createTemporaryFile("parsefiledeb822.XXXXXX.sources",
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

   pkgSourceList sources;
   EXPECT_TRUE(sources.Read(file.Name()));
   EXPECT_EQ(2u, sources.size());
}
