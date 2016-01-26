#include <apt-pkg/error.h>
#include <apt-pkg/extracttar.h>
#include <iostream>
#include <stdlib.h>

#include <gtest/gtest.h>
#include "assert.h"

class Stream : public pkgDirStream
{
   public:
    int count;
    Stream () { count = 0; }
    virtual bool DoItem(Item &Itm,int &Fd) { (void)Itm; (void)Fd; count++; return true; }
    virtual bool Fail(Item &Itm,int Fd) { (void)Itm; (void)Fd; return true; }
    virtual bool FinishedFile(Item &Itm,int Fd) { (void)Itm; (void)Fd; return true; }
    virtual bool Process(Item &Itm,const unsigned char * Data, unsigned long Size,unsigned long Pos) { (void)Itm; (void) Data; (void) Size; (void) Pos; return true; }
    virtual ~Stream() {}
};

TEST(ExtractTar, ExtractTar)
{
    EXPECT_EQ(system("tar c /etc/passwd 2>/dev/null | gzip > tar.tgz"), 0);

    FileFd fd("tar.tgz", FileFd::ReadOnly);
    unlink("tar.tgz");
    ExtractTar tar(fd, -1, "gzip");

    // Run multiple times, because we want to check not only that extraction
    // works, but also that it works multiple times (important for python-apt)
    for (int i = 0; i < 5; i++) {
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
