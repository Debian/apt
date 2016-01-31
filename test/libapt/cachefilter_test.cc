#include <config.h>

#include <apt-pkg/cachefilter.h>

#include <string>

#include <gtest/gtest.h>

TEST(CacheFilterTest, ArchitectureSpecification)
{
   {
      SCOPED_TRACE("Pattern is any-armhf");
      APT::CacheFilter::PackageArchitectureMatchesSpecification ams("any-armhf");
      EXPECT_TRUE(ams("armhf"));
      EXPECT_FALSE(ams("armel"));
      EXPECT_TRUE(ams("linux-armhf"));
      EXPECT_FALSE(ams("linux-armel"));
      EXPECT_TRUE(ams("kfreebsd-armhf"));
      EXPECT_TRUE(ams("gnu-linux-armhf"));
      EXPECT_FALSE(ams("gnu-linux-armel"));
      EXPECT_TRUE(ams("gnu-kfreebsd-armhf"));
      EXPECT_TRUE(ams("musl-linux-armhf"));
   }
   {
      SCOPED_TRACE("Pattern is linux-any");
      APT::CacheFilter::PackageArchitectureMatchesSpecification ams("linux-any");
      EXPECT_TRUE(ams("armhf"));
      EXPECT_TRUE(ams("armel"));
      EXPECT_TRUE(ams("linux-armhf"));
      EXPECT_TRUE(ams("linux-armel"));
      EXPECT_FALSE(ams("kfreebsd-armhf"));
      EXPECT_TRUE(ams("gnu-linux-armhf"));
      EXPECT_TRUE(ams("gnu-linux-armel"));
      EXPECT_FALSE(ams("gnu-kfreebsd-armhf"));
      EXPECT_TRUE(ams("musl-linux-armhf"));
   }
   {
      SCOPED_TRACE("Pattern is gnu-any-any");
      APT::CacheFilter::PackageArchitectureMatchesSpecification ams("gnu-any-any"); //really?
      EXPECT_TRUE(ams("armhf"));
      EXPECT_TRUE(ams("armel"));
      EXPECT_TRUE(ams("linux-armhf"));
      EXPECT_TRUE(ams("linux-armel"));
      EXPECT_TRUE(ams("kfreebsd-armhf"));
      EXPECT_TRUE(ams("gnu-linux-armhf"));
      EXPECT_TRUE(ams("gnu-linux-armel"));
      EXPECT_TRUE(ams("gnu-kfreebsd-armhf"));
      EXPECT_FALSE(ams("musl-linux-armhf"));
   }
   {
      SCOPED_TRACE("Architecture is armhf");
      APT::CacheFilter::PackageArchitectureMatchesSpecification ams("armhf", false);
      EXPECT_TRUE(ams("armhf"));
      EXPECT_FALSE(ams("armel"));
      EXPECT_TRUE(ams("linux-any"));
      EXPECT_FALSE(ams("kfreebsd-any"));
      EXPECT_TRUE(ams("any-armhf"));
      EXPECT_FALSE(ams("any-armel"));
      EXPECT_TRUE(ams("linux-armhf"));
      EXPECT_FALSE(ams("kfreebsd-armhf"));
      EXPECT_TRUE(ams("gnu-linux-armhf"));
      EXPECT_FALSE(ams("gnu-linux-armel"));
      EXPECT_FALSE(ams("gnu-kfreebsd-armhf"));
      EXPECT_FALSE(ams("musl-linux-armhf"));
   }
}
