#include <config.h>
#include <apt-pkg/hashes.h>
#include <apt-pkg/acquire.h>
#include <apt-pkg/acquire-item.h>
#include <apt-pkg/configuration.h>
#include <apt-private/acqprogress.h>
#include <string>
#include <sstream>
#include <gtest/gtest.h>

class TestItem: public pkgAcquire::Item
{
public:
   explicit TestItem(pkgAcquire * const Acq) : pkgAcquire::Item(Acq) {}

   virtual std::string DescURI() const APT_OVERRIDE { return ""; }
   virtual HashStringList GetExpectedHashes() const APT_OVERRIDE { return HashStringList(); }

};

TEST(AcqProgress, IMSHit)
{
   std::ostringstream out;
   unsigned int width = 80;
   AcqTextStatus Stat(out, width, 0);
   Stat.Start();

   pkgAcquire Acq(&Stat);
   pkgAcquire::ItemDesc hit;
   hit.URI = "http://example.org/file";
   hit.Description = "Example File from example.org";
   hit.ShortDesc = "Example File";
   TestItem hitO(&Acq);
   hit.Owner = &hitO;

   EXPECT_EQ("", out.str());
   Stat.IMSHit(hit);
   EXPECT_EQ("Hit:1 Example File from example.org\n", out.str());
   Stat.IMSHit(hit);
   EXPECT_EQ("Hit:1 Example File from example.org\n"
	     "Hit:1 Example File from example.org\n", out.str());
   Stat.Stop();
   EXPECT_EQ("Hit:1 Example File from example.org\n"
	     "Hit:1 Example File from example.org\n", out.str());
}
TEST(AcqProgress, FetchNoFileSize)
{
   std::ostringstream out;
   unsigned int width = 80;
   AcqTextStatus Stat(out, width, 0);
   Stat.Start();

   pkgAcquire Acq(&Stat);
   pkgAcquire::ItemDesc fetch;
   fetch.URI = "http://example.org/file";
   fetch.Description = "Example File from example.org";
   fetch.ShortDesc = "Example File";
   TestItem fetchO(&Acq);
   fetch.Owner = &fetchO;

   EXPECT_EQ("", out.str());
   Stat.Fetch(fetch);
   EXPECT_EQ("Get:1 Example File from example.org\n", out.str());
   Stat.Fetch(fetch);
   EXPECT_EQ("Get:1 Example File from example.org\n"
	     "Get:1 Example File from example.org\n", out.str());
   Stat.Stop();
   EXPECT_EQ("Get:1 Example File from example.org\n"
	     "Get:1 Example File from example.org\n", out.str());
}
TEST(AcqProgress, FetchFileSize)
{
   std::ostringstream out;
   unsigned int width = 80;
   AcqTextStatus Stat(out, width, 0);
   Stat.Start();

   pkgAcquire Acq(&Stat);
   pkgAcquire::ItemDesc fetch;
   fetch.URI = "http://example.org/file";
   fetch.Description = "Example File from example.org";
   fetch.ShortDesc = "Example File";
   TestItem fetchO(&Acq);
   fetchO.FileSize = 100;
   fetch.Owner = &fetchO;

   EXPECT_EQ("", out.str());
   Stat.Fetch(fetch);
   EXPECT_EQ("Get:1 Example File from example.org [100 B]\n", out.str());
   fetchO.FileSize = 42;
   Stat.Fetch(fetch);
   EXPECT_EQ("Get:1 Example File from example.org [100 B]\n"
	     "Get:1 Example File from example.org [42 B]\n", out.str());
   Stat.Stop();
   EXPECT_EQ("Get:1 Example File from example.org [100 B]\n"
	     "Get:1 Example File from example.org [42 B]\n", out.str());
}
TEST(AcqProgress, Fail)
{
   std::ostringstream out;
   unsigned int width = 80;
   AcqTextStatus Stat(out, width, 0);
   Stat.Start();

   pkgAcquire Acq(&Stat);
   pkgAcquire::ItemDesc fetch;
   fetch.URI = "http://example.org/file";
   fetch.Description = "Example File from example.org";
   fetch.ShortDesc = "Example File";
   TestItem fetchO(&Acq);
   fetchO.FileSize = 100;
   fetchO.Status = pkgAcquire::Item::StatIdle;
   fetch.Owner = &fetchO;

   EXPECT_EQ("", out.str());
   Stat.Fail(fetch);
   EXPECT_EQ("Ign:1 Example File from example.org\n", out.str());
   fetchO.Status = pkgAcquire::Item::StatDone;
   Stat.Fail(fetch);
   EXPECT_EQ("Ign:1 Example File from example.org\n"
	     "Ign:1 Example File from example.org\n", out.str());
   fetchO.Status = pkgAcquire::Item::StatError;
   fetchO.ErrorText = "An error test!";
   Stat.Fail(fetch);
   EXPECT_EQ("Ign:1 Example File from example.org\n"
	     "Ign:1 Example File from example.org\n"
	     "Err:1 Example File from example.org\n"
	     "  An error test!\n", out.str());
   _config->Set("Acquire::Progress::Ignore::ShowErrorText", true);
   fetchO.Status = pkgAcquire::Item::StatDone;
   Stat.Fail(fetch);
   EXPECT_EQ("Ign:1 Example File from example.org\n"
	     "Ign:1 Example File from example.org\n"
	     "Err:1 Example File from example.org\n"
	     "  An error test!\n"
	     "Ign:1 Example File from example.org\n"
	     "  An error test!\n", out.str());
   _config->Set("Acquire::Progress::Ignore::ShowErrorText", true);
   Stat.Stop();
   EXPECT_EQ("Ign:1 Example File from example.org\n"
	     "Ign:1 Example File from example.org\n"
	     "Err:1 Example File from example.org\n"
	     "  An error test!\n"
	     "Ign:1 Example File from example.org\n"
	     "  An error test!\n", out.str());
}
TEST(AcqProgress, Pulse)
{
   std::ostringstream out;
   unsigned int width = 80;
   AcqTextStatus Stat(out, width, 0);
   _config->Set("APT::Sandbox::User", ""); // ensure we aren't sandboxing

   pkgAcquire Acq(&Stat);
   pkgAcquire::ItemDesc fetch;
   fetch.URI = "http://example.org/file";
   fetch.Description = "Example File from example.org";
   fetch.ShortDesc = "Example File";
   TestItem fetchO(&Acq);
   fetchO.FileSize = 100;
   fetchO.Status = pkgAcquire::Item::StatFetching;
   fetch.Owner = &fetchO;

   // make screen smaller and bigger again while running
   EXPECT_TRUE(Stat.Pulse(&Acq));
   EXPECT_EQ("\r0% [Working]", out.str());
   width = 8;
   EXPECT_TRUE(Stat.Pulse(&Acq));
   EXPECT_EQ("\r0% [Working]"
	     "\r        "
	     "\r0% [Work", out.str());
   width = 80;
   EXPECT_TRUE(Stat.Pulse(&Acq));
   EXPECT_EQ("\r0% [Working]"
	     "\r        "
	     "\r0% [Work"
	     "\r0% [Working]", out.str());
}
