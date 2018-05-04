#include <config.h>

#include <apt-pkg/error.h>
#include <apt-pkg/fileutl.h>

#include <iostream>
#include <string>
#include <vector>

#include <gtest/gtest.h>

#include "file-helpers.h"

#define P(x)	tempdir + "/" + x

TEST(FileUtlTest,GetListOfFilesInDir)
{
   std::string tempdir;
   createTemporaryDirectory("getlistoffiles", tempdir);

   createFile(tempdir, "anormalfile");
   createFile(tempdir, "01yet-anothernormalfile");
   createFile(tempdir, "anormalapt.conf");
   createFile(tempdir, "01yet-anotherapt.conf");
   createFile(tempdir, "anormalapt.list");
   createFile(tempdir, "01yet-anotherapt.list");
   createFile(tempdir, "wrongextension.wron");
   createFile(tempdir, "wrong-extension.wron");
   createFile(tempdir, "strangefile.");
   createFile(tempdir, "s.t.r.a.n.g.e.f.i.l.e");
   createFile(tempdir, ".hiddenfile");
   createFile(tempdir, ".hiddenfile.conf");
   createFile(tempdir, ".hiddenfile.list");
   createFile(tempdir, "multi..dot");
   createFile(tempdir, "multi.dot.conf");
   createFile(tempdir, "multi.dot.list");
   createFile(tempdir, "disabledfile.disabled");
   createFile(tempdir, "disabledfile.conf.disabled");
   createFile(tempdir, "disabledfile.list.disabled");
   createFile(tempdir, "invälid.conf");
   createFile(tempdir, "invalíd");
   createFile(tempdir, "01invalíd");
   createDirectory(tempdir, "invaliddir");
   createDirectory(tempdir, "directory.conf");
   createDirectory(tempdir, "directory.list");
   createDirectory(tempdir, "directory.wron");
   createDirectory(tempdir, "directory.list.disabled");
   createLink(tempdir, "anormalfile", "linkedfile.list");
   createLink(tempdir, "invaliddir", "linkeddir.list");
   createLink(tempdir, "non-existing-file", "brokenlink.list");

   // Files with no extension
   _error->PushToStack();
   std::vector<std::string> files = GetListOfFilesInDir(tempdir, "", true);
   ASSERT_EQ(2u, files.size());
   EXPECT_EQ(P("01yet-anothernormalfile"), files[0]);
   EXPECT_EQ(P("anormalfile"), files[1]);

   // Files with no extension - should be the same as above
   files = GetListOfFilesInDir(tempdir, "", true, true);
   ASSERT_EQ(2u, files.size());
   EXPECT_EQ(P("01yet-anothernormalfile"), files[0]);
   EXPECT_EQ(P("anormalfile"), files[1]);

   // Files with impossible extension
   files = GetListOfFilesInDir(tempdir, "impossible", true);
   EXPECT_TRUE(files.empty());

   // Files with impossible or no extension
   files = GetListOfFilesInDir(tempdir, "impossible", true, true);
   ASSERT_EQ(2u, files.size());
   EXPECT_EQ(P("01yet-anothernormalfile"), files[0]);
   EXPECT_EQ(P("anormalfile"), files[1]);

   // Files with list extension - nothing more
   files = GetListOfFilesInDir(tempdir, "list", true);
   ASSERT_EQ(4u, files.size());
   EXPECT_EQ(P("01yet-anotherapt.list"), files[0]);
   EXPECT_EQ(P("anormalapt.list"), files[1]);
   EXPECT_EQ(P("linkedfile.list"), files[2]);
   EXPECT_EQ(P("multi.dot.list"), files[3]);

   // Files with conf or no extension
   files = GetListOfFilesInDir(tempdir, "conf", true, true);
   ASSERT_EQ(5u, files.size());
   EXPECT_EQ(P("01yet-anotherapt.conf"), files[0]);
   EXPECT_EQ(P("01yet-anothernormalfile"), files[1]);
   EXPECT_EQ(P("anormalapt.conf"), files[2]);
   EXPECT_EQ(P("anormalfile"), files[3]);
   EXPECT_EQ(P("multi.dot.conf"), files[4]);

   // Files with disabled extension - nothing more
   files = GetListOfFilesInDir(tempdir, "disabled", true);
   ASSERT_EQ(3u, files.size());
   EXPECT_EQ(P("disabledfile.conf.disabled"), files[0]);
   EXPECT_EQ(P("disabledfile.disabled"), files[1]);
   EXPECT_EQ(P("disabledfile.list.disabled"), files[2]);

   // Files with disabled or no extension
   files = GetListOfFilesInDir(tempdir, "disabled", true, true);
   ASSERT_EQ(5u, files.size());
   EXPECT_EQ(P("01yet-anothernormalfile"), files[0]);
   EXPECT_EQ(P("anormalfile"), files[1]);
   EXPECT_EQ(P("disabledfile.conf.disabled"), files[2]);
   EXPECT_EQ(P("disabledfile.disabled"), files[3]);
   EXPECT_EQ(P("disabledfile.list.disabled"), files[4]);

   // discard the unknown file extension messages
   if (_error->PendingError())
      _error->MergeWithStack();
   else
      _error->RevertToStack();
   removeDirectory(tempdir);
}
