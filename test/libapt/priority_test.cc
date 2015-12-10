#include <config.h>
#include <apt-pkg/pkgcache.h>
#include <string>
#include <gtest/gtest.h>

using std::string;

// Tests for Bug#807523
TEST(PriorityTest, PriorityPrinting)
{
   EXPECT_EQ("required", string(pkgCache::Priority(pkgCache::State::Required)));
   EXPECT_EQ("important", string(pkgCache::Priority(pkgCache::State::Important)));
   EXPECT_EQ("standard", string(pkgCache::Priority(pkgCache::State::Standard)));
   EXPECT_EQ("optional", string(pkgCache::Priority(pkgCache::State::Optional)));
   EXPECT_EQ("extra", string(pkgCache::Priority(pkgCache::State::Extra)));
}
