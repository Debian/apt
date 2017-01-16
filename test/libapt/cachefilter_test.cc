#include <config.h>

#include <apt-pkg/cachefilter.h>
#include <apt-pkg/fileutl.h>

#include <string>

#include <gtest/gtest.h>

TEST(CacheFilterTest, ArchitectureSpecification)
{
   {
      SCOPED_TRACE("Pattern is *");
      // * should be treated like any
      APT::CacheFilter::PackageArchitectureMatchesSpecification ams("*");
      EXPECT_TRUE(ams("sparc"));
      EXPECT_TRUE(ams("amd64"));
      EXPECT_TRUE(ams("kfreebsd-amd64"));
   }
   {
      SCOPED_TRACE("Pattern is any-i386");
      APT::CacheFilter::PackageArchitectureMatchesSpecification ams("any-i386");
      EXPECT_TRUE(ams("i386"));
      EXPECT_FALSE(ams("amd64"));
      EXPECT_TRUE(ams("linux-i386"));
      EXPECT_FALSE(ams("linux-amd64"));
      EXPECT_TRUE(ams("kfreebsd-i386"));
      EXPECT_TRUE(ams("musl-linux-i386"));
   }
   {
      SCOPED_TRACE("Pattern is linux-any");
      APT::CacheFilter::PackageArchitectureMatchesSpecification ams("linux-any");
      EXPECT_TRUE(ams("armhf"));
      EXPECT_TRUE(ams("armel"));
      EXPECT_TRUE(ams("linux-armhf"));
      EXPECT_TRUE(ams("linux-armel"));
      EXPECT_FALSE(ams("kfreebsd-armhf"));
      EXPECT_TRUE(ams("musl-linux-armhf"));
   }
   if (FileExists(DPKG_DATADIR "/tupletable"))
   {
      SCOPED_TRACE("Pattern is gnu-any-any");
      APT::CacheFilter::PackageArchitectureMatchesSpecification ams("gnu-any-any"); //really?
      EXPECT_TRUE(ams("armhf"));
      EXPECT_TRUE(ams("armel"));
      EXPECT_TRUE(ams("linux-armhf"));
      EXPECT_TRUE(ams("linux-armel"));
      EXPECT_TRUE(ams("kfreebsd-armhf"));
      EXPECT_FALSE(ams("musl-linux-armhf"));
   }
   if (FileExists(DPKG_DATADIR "/triplettable"))
   {
      SCOPED_TRACE("Pattern is gnueabi-any-any");
      APT::CacheFilter::PackageArchitectureMatchesSpecification ams("gnueabi-any-any"); //really?
      EXPECT_TRUE(ams("linux-armel"));
      EXPECT_TRUE(ams("armel"));
      EXPECT_FALSE(ams("armhf"));
      EXPECT_FALSE(ams("linux-armhf"));
      EXPECT_FALSE(ams("musleabihf-linux-armhf"));
   }
   if (FileExists(DPKG_DATADIR "/triplettable"))
   {
      SCOPED_TRACE("Pattern is gnueabihf-any-any");
      APT::CacheFilter::PackageArchitectureMatchesSpecification ams("gnueabihf-any-any"); //really?
      EXPECT_FALSE(ams("linux-armel"));
      EXPECT_FALSE(ams("armel"));
      EXPECT_TRUE(ams("armhf"));
      EXPECT_TRUE(ams("linux-armhf"));
      EXPECT_FALSE(ams("musleabihf-linux-armhf"));
   }

   // Weird ones - armhf's tuple is actually eabihf-gnu-linux-arm
   //              armel's tuple is actually eabi-gnu-linux-arm
   //              x32's   tuple is actually x32-gnu-linux-amd64
   {
      SCOPED_TRACE("Architecture is armhf");
      APT::CacheFilter::PackageArchitectureMatchesSpecification ams("armhf", false);
      EXPECT_TRUE(ams("armhf"));
      EXPECT_FALSE(ams("armel"));
      EXPECT_TRUE(ams("linux-any"));
      EXPECT_FALSE(ams("kfreebsd-any"));
      EXPECT_TRUE(ams("any-arm"));
      EXPECT_TRUE(ams("linux-armhf"));
      EXPECT_FALSE(ams("kfreebsd-armhf"));
      EXPECT_FALSE(ams("musl-linux-armhf"));
   }
   {
      SCOPED_TRACE("Pattern is any-arm");
      APT::CacheFilter::PackageArchitectureMatchesSpecification ams("any-arm");
      EXPECT_TRUE(ams("armhf"));
      EXPECT_TRUE(ams("armel"));
      EXPECT_TRUE(ams("linux-armhf"));
      EXPECT_TRUE(ams("linux-armel"));
      EXPECT_TRUE(ams("musl-linux-armhf"));
      EXPECT_TRUE(ams("uclibc-linux-armel"));

      EXPECT_FALSE(ams("arm64"));
      EXPECT_FALSE(ams("linux-arm64"));
      EXPECT_FALSE(ams("kfreebsd-arm64"));
      EXPECT_FALSE(ams("musl-linux-arm64"));
   }
   {
      SCOPED_TRACE("Pattern is any-amd64");
      APT::CacheFilter::PackageArchitectureMatchesSpecification ams("any-amd64");
      EXPECT_TRUE(ams("amd64"));
      EXPECT_TRUE(ams("x32"));
      EXPECT_TRUE(ams("linux-amd64"));
      EXPECT_TRUE(ams("linux-x32"));
      EXPECT_TRUE(ams("kfreebsd-amd64"));
      EXPECT_TRUE(ams("musl-linux-amd64"));
      EXPECT_TRUE(ams("uclibc-linux-amd64"));

      EXPECT_FALSE(ams("i386"));
      EXPECT_FALSE(ams("linux-i386"));
      EXPECT_FALSE(ams("kfreebsd-i386"));
      EXPECT_FALSE(ams("musl-linux-i386"));
      EXPECT_FALSE(ams("uclibc-linux-i386"));
   }
}
