#include <apt-pkg/error.h>
#include <apt-pkg/dirstream.h>
#include <apt-pkg/extracttar.h>
#include <iostream>
#include <stdlib.h>

#include "assert.h"
#include <gtest/gtest.h>

class Stream : public pkgDirStream
{
   public:
    int count;
    Stream () { count = 0; }
    bool DoItem(Item &Itm,int &Fd) APT_OVERRIDE { (void)Itm; (void)Fd; count++; return true; }
    bool Fail(Item &Itm,int Fd) APT_OVERRIDE { (void)Itm; (void)Fd; return true; }
    bool FinishedFile(Item &Itm,int Fd) APT_OVERRIDE { (void)Itm; (void)Fd; return true; }
    ~Stream() {}
};

TEST(ExtractTar, ExtractTar)
{
   FileFd tgz;
   ASSERT_NE(nullptr, GetTempFile("extracttar", false, &tgz));
   ASSERT_TRUE(tgz.Close());
   ASSERT_FALSE(tgz.Name().empty());
   // FIXME: We should do the right thing… but its a test and nobody will ever…
   // Proposal: The first one who sees this assert fail will have to write a patch.
   ASSERT_EQ(std::string::npos, tgz.Name().find('\''));
   EXPECT_EQ(0, system(("tar c /etc/passwd 2>/dev/null | gzip > " + tgz.Name()).c_str()));

    FileFd fd(tgz.Name(), FileFd::ReadOnly);
    RemoveFile("ExtractTarTest", tgz.Name());
    ASSERT_TRUE(fd.IsOpen());
    ExtractTar tar(fd, -1, "gzip");

    // Run multiple times, because we want to check not only that extraction
    // works, but also that it works multiple times (important for python-apt)
    for (int i = 0; i < 5; i++) {
       SCOPED_TRACE(i);
        Stream stream;
        fd.Seek(0);
        tar.Go(stream);
        if (_error->PendingError()) {
            _error->DumpErrors();
            EXPECT_FALSE(true);
        }
        EXPECT_EQ(stream.count, 1);
    }
}
