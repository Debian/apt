#include <config.h>

#include <apt-pkg/sourcelist.h>
#include <apt-pkg/fileutl.h>

#include <string>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <gtest/gtest.h>

#include "file-helpers.h"

class SourceList : public pkgSourceList {
   public:
      using pkgSourceList::ParseFileDeb822;
};

TEST(SourceListTest,ParseFileDeb822)
{
   FileFd fd;
   char * tempfile;
   createTemporaryFile("parsefiledeb822", fd, &tempfile,
      "Types: deb\n"
      "URIs: http://ftp.debian.org/debian\n"
      "Suites: stable\n"
      "Sections: main\n"
      "Description: short\n"
      " long description that can be very long\n"
      "\n"
      "Types: deb\n"
      "URIs: http://ftp.debian.org/debian\n"
      "Suites: unstable\n"
      "Sections: main non-free\n");
   fd.Close();

   SourceList sources;
   EXPECT_EQ(2, sources.ParseFileDeb822(tempfile));
   EXPECT_EQ(2, sources.size());

   unlink(tempfile);
}
